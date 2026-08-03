# Stochastic Computing Backend

## Overview

Quanti now supports two computational backends for probabilistic operations:

1. **Float Backend** (default): Uses traditional floating-point arithmetic with probability distributions
2. **Stochastic Computing (SC) Backend**: Uses bit-streams where probability is encoded as the frequency of 1s

## Architecture

### Backend Selection

Each `KaruByte` can independently choose its backend:

```c
// Float backend (default)
KaruByte k1 = quanti_prob(rt, dist);  // uses dist_intersect for AND

// SC backend
KaruByte k2 = quanti_prob_sc(rt, 0.7);  // uses sc_and for AND

// Switch backend at runtime
quanti_set_backend(rt, &k1, BACKEND_SC);  // convert float → SC
quanti_set_backend(rt, &k2, BACKEND_FLOAT);  // convert SC → float
```

### Auto-detection

When performing operations (`karu_and`, `karu_or`, `karu_not`), Quanti automatically detects the backend:
- If both operands are SC → result is SC
- If one is SC and one is Float → converts Float to SC, result is SC
- If both are Float → result is Float

## Key Differences

### Semantic Difference

The two backends implement **different mathematical operations**:

| Operation | Float Backend | SC Backend |
|-----------|---------------|------------|
| AND | Bayesian intersection: `P(A∩B) / (P(A∩B) + P(¬A∩¬B))` | Raw multiplication: `P(A) × P(B)` |
| OR | Bayesian union | `P(A) + P(B) - P(A)×P(B)` |
| NOT | Complement distribution | Bit inversion: `1 - P` |

**Example:**
```
AND(0.7, 0.8):
  Float → 0.9032 (Bayesian: 0.56 / (0.56 + 0.06))
  SC    → 0.5600 (Raw: 0.7 × 0.8)
```

### When to Use Each

**Use Float Backend when:**
- You need exact precision
- You're doing Bayesian reasoning (updating beliefs)
- You want deterministic results

**Use SC Backend when:**
- You need raw probabilistic multiplication (independence assumption)
- You want tolerance to imprecision (fault-tolerant computing)
- You're implementing neural network-like operations
- You plan to port to FPGA/ASIC hardware later

## API Reference

### StochasticStream

```c
// Create a stream
StochasticStream *sc = sc_create(0.7, 1024, SC_RANDOM);

// Operations (bit-level)
StochasticStream *result_and = sc_and(stream_a, stream_b);
StochasticStream *result_or  = sc_or(stream_a, stream_b);
StochasticStream *result_not = sc_not(stream_a);

// Read probability
double p = sc_estimate(sc);  // count_ones / length

// Progressive reading with convergence detection
double p_converged = sc_estimate_converging(sc, 0.95, 4096);

// Convert to/from Distribution
StochasticStream *sc = sc_from_distribution(dist, 1024);
Distribution *dist = sc_to_distribution(sc);
```

### Sequence Types

```c
typedef enum {
    SC_RANDOM,          // PRNG-based (default, best independence)
    SC_VAN_DER_CORPUT,  // Low-discrepancy (faster convergence, single stream)
    SC_LFSR             // Linear Feedback Shift Register
} SCSequenceType;
```

**Recommendation:** Use `SC_RANDOM` for operations between multiple streams (better independence). Use `SC_VAN_DER_CORPUT` for single-stream estimation (faster convergence).

### Runtime Configuration

```c
QuantiConfig cfg = quanti_default_config();
cfg.default_backend = BACKEND_SC;        // or BACKEND_FLOAT
cfg.sc_stream_length = 1024;             // bits per stream
cfg.sc_sequence_type = SC_RANDOM;        // sequence generator

QuantiRuntime *rt = quanti_init(cfg);
```

## Performance (10,000 operations, 1024-bit streams)

| Operation | Float Backend | SC Backend | Ratio |
|-----------|---------------|------------|-------|
| Create 10,000 KaruBytes | 2.3 ms | 132 ms | 56x slower |
| 5,000 AND | 283 ms | 374 ms | 1.3x slower |
| 5,000 OR | 372 ms | 430 ms | 1.2x slower |
| 10,000 NOT | 732 ms | 707 ms | 0.97x (similar) |

**Analysis:**
- SC creation is slower (must generate bit streams)
- SC operations are comparable in speed (bit operations are fast)
- The creation cost is amortized over many operations

## Accuracy (1024-bit streams)

| Operation | Expected | SC Result | Error |
|-----------|----------|-----------|-------|
| AND(0.7, 0.8) | 0.5600 | 0.5342 | 0.026 |
| AND(0.3, 0.4) | 0.1200 | 0.1064 | 0.014 |
| AND(0.5, 0.5) | 0.2500 | 0.2510 | 0.001 |
| AND(0.9, 0.1) | 0.0900 | 0.0918 | 0.002 |
| AND(0.6, 0.6) | 0.3600 | 0.3555 | 0.005 |

**Precision:** ~2-3 decimal places with 1024 bits. Increase `sc_stream_length` for higher precision (scales as O(1/√N)).

## Examples

### Example 1: Basic SC Operations

```c
QuantiConfig cfg = quanti_default_config();
cfg.default_backend = BACKEND_SC;
cfg.sc_stream_length = 4096;
QuantiRuntime *rt = quanti_init(cfg);

// Create probabilistic KaruBytes
KaruByte a = quanti_prob_sc(rt, 0.7);  // P(A) = 0.7
KaruByte b = quanti_prob_sc(rt, 0.8);  // P(B) = 0.8

// AND: P(A∩B) = P(A) × P(B) = 0.56
KaruByte c = quanti_and(rt, a, b);
double p_c = sc_estimate(c.stream);  // ≈ 0.56

// OR: P(A∪B) = P(A) + P(B) - P(A)×P(B) = 0.94
KaruByte d = quanti_or(rt, a, b);
double p_d = sc_estimate(d.stream);  // ≈ 0.94

// NOT: P(¬A) = 1 - P(A) = 0.3
KaruByte e = quanti_not(rt, a);
double p_e = sc_estimate(e.stream);  // ≈ 0.3

quanti_destroy(rt);
```

### Example 2: Hybrid Backend

```c
QuantiRuntime *rt = quanti_init(quanti_default_config());

// Start with Float backend
Distribution *d = dist_discrete((double[]){0.6, 0.4}, NULL, 2);
KaruByte k = quanti_prob(rt, d);
assert(k.backend == BACKEND_FLOAT);

// Convert to SC
quanti_set_backend(rt, &k, BACKEND_SC);
assert(k.backend == BACKEND_SC);
assert(k.stream != NULL);

// Now operations use SC semantics
KaruByte k2 = quanti_prob_sc(rt, 0.5);
KaruByte result = quanti_and(rt, k, k2);
assert(result.backend == BACKEND_SC);

quanti_destroy(rt);
```

### Example 3: Progressive Convergence

```c
StochasticStream *sc = sc_create(0.7, 10000, SC_VAN_DER_CORPUT);

// Read at different confidence levels
double p_low  = sc_estimate_converging(sc, 0.90, 10000);  // fast, low confidence
double p_med  = sc_estimate_converging(sc, 0.95, 10000);  // balanced
double p_high = sc_estimate_converging(sc, 0.99, 10000);  // slow, high confidence

printf("P ≈ %.3f (90%% confidence)\n", p_low);
printf("P ≈ %.3f (95%% confidence)\n", p_med);
printf("P ≈ %.3f (99%% confidence)\n", p_high);

sc_free(sc);
```

## Testing

```bash
# Run all tests
./build/test_karubyte.exe     # 63 tests (including SC backend)
./build/test_stochastic.exe   # 33 tests (SC module)
./build/test_runtime.exe      # 20 tests (including SC runtime)

# Run benchmark
./build/benchmark_sc.exe
```

## Future Work

1. **FPGA Implementation**: Port SC operations to hardware for massive parallelism
2. **Correlated SC**: Implement correlation-aware operations for dependent variables
3. **Adaptive Precision**: Automatically adjust stream length based on required accuracy
4. **Mixed Representations**: Use Float for high-precision nodes, SC for low-precision nodes
