# Quanti

**Multi-State Runtime Programming — un lenguaje y runtime donde la incertidumbre es una primitiva nativa.**

Quanti es un sistema computacional experimental compuesto por:

- **QA** — un lenguaje de programación con tipos multiestado nativos (`karu`)
- **Quanti Runtime** — un motor C que mantiene ramas de ejecución paralelas, propaga incertidumbre por un DAG de dependencias, y colapsa solo cuando es necesario
- **Experimento de cuantización** — un framework Python que aplica la filosofía de "colapso tardío" (via stochastic rounding + ensemble) a la cuantización de LLMs reales

La hipótesis central: *un runtime multiestado puede mejorar la precisión de modelos cuantizados manteniendo el ruido de cuantización como incertidumbre no colapsada, y promediándolo recién en el espacio de output (logits) en vez de colapsar deterministicamente peso por peso.*

---

## Tabla de Contenidos

- [Quick Start](#quick-start)
- [KaruByte — La Unidad Lógica](#karubyte--la-unidad-lógica)
  - [Los 5 Estados](#los-5-estados)
  - [Álgebra AND / OR / NOT](#álgebra-and--or--not)
  - [Distribuciones](#distribuciones)
  - [Colapso: map / sample / first](#colapso-map--sample--first)
- [Lenguaje QA](#lenguaje-qa)
  - [Tipos](#tipos)
  - [Declaraciones y control de flujo](#declaraciones-y-control-de-flujo)
  - [Distribuciones](#distribuciones-en-qa)
  - [Ejemplo completo](#ejemplo-completo)
- [Arquitectura de la Biblioteca C](#arquitectura-de-la-biblioteca-c)
  - [KaruByte → Memory → Collapse → Branch → Runtime](#karubyte--memory--collapse--branch--runtime)
  - [DAG de dependencias](#dag-de-dependencias)
  - [Colapso propagante](#colapso-propagante)
  - [Branching y merge](#branching-y-merge)
- [Experimento de Cuantización de LLMs](#experimento-de-cuantización-de-llms)
  - [Filosofía: late collapse](#filosofía-late-collapse)
  - [RTN-INT4 (measure:map) vs Stochastic-INT4 (measure:sample)](#rtn-int4-measuremap-vs-stochastic-int4-measuresample)
  - [Uso](#uso)
  - [Resultados](#resultados)
- [Test Suite](#test-suite)
  - [Tests unitarios C (126 tests)](#tests-unitarios-c-126-tests)
  - [Tests end-to-end QA](#tests-end-to-end-qa)
- [Estructura del Proyecto](#estructura-del-proyecto)
- [Build](#build)

---

## Quick Start

```bash
# Compilar todo (MSYS2/MinGW: mingw32-make)
mingw32-make

# Ejecutar un programa .qa
./build/quanti examples/demo.qa

# Output esperado (I/O colapsa karu con measure:map):
#   Quanti
#   42
#   1          ← print(superposition) colapsa a map
#   1
#   ...
```

### Compilar a binario nativo (AOT)

```bash
make libquanti
./build/quanti.exe build examples/classical.qa -o build/classical.exe --lto
./build/classical.exe

./build/quanti.exe vm examples/demo.qa
./build/quanti.exe ir examples/demo.qa
```

---

## KaruByte — La Unidad Lógica

Un `KaruByte` es un valor único que puede encontrarse en **5 estados simultáneos posibles**. Es análogo conceptual a un qubit, pero implementado en software clásico.

### Los 5 Estados

| Estado | Símbolo | Significado |
|--------|---------|-------------|
| Falso | `0` | Valor determinista falso |
| Verdadero | `1` | Valor determinista verdadero |
| Superposición | `K` | Ambos valores simultáneamente activos |
| Indefinido | `Ø` | Estado desconocido, no resuelto |
| Probabilístico | `P(d)` | Distribución de probabilidad `d` asociada |

### Álgebra AND / OR / NOT

#### AND

| AND | `0` | `1` | `K` | `Ø` | `P` |
|-----|-----|-----|-----|-----|-----|
| `0` | `0` | `0` | `0` | `0` | `0` |
| `1` | `0` | `1` | `K` | `Ø` | `P` |
| `K` | `0` | `K` | `K` | `Ø` | `K∩P` |
| `Ø` | `0` | `Ø` | `Ø` | `Ø` | `Ø` |
| `P` | `0` | `P` | `K∩P` | `Ø` | `P·P'` |

Reglas clave:
- `0 AND X = 0` — el falso absorbe
- `Ø AND X = Ø` (excepto con 0) — lo indefinido contamina en AND
- `P AND P = P·P'` — intersección de distribuciones

#### OR

| OR | `0` | `1` | `K` | `Ø` | `P` |
|----|-----|-----|-----|-----|-----|
| `0` | `0` | `1` | `K` | `Ø` | `P` |
| `1` | `1` | `1` | `1` | `1` | `1` |
| `K` | `K` | `1` | `K` | `K∪Ø` | `K∪P` |
| `Ø` | `Ø` | `1` | `K∪Ø` | `Ø` | `Ø∪P` |
| `P` | `P` | `1` | `K∪P` | `Ø∪P` | `P+P'` |

Reglas clave:
- `1 OR X = 1` — la certeza absorbe
- `P OR P = P+P'` — unión de distribuciones

#### NOT

| X | NOT X |
|---|-------|
| `0` | `1` |
| `1` | `0` |
| `K` | `K` (inversión en ambas ramas) |
| `Ø` | `Ø` |
| `P(d)` | `P(1-d)` (distribución complementada) |

### Distribuciones

El estado `P` lleva asociada una **distribución de probabilidad completa**:

```c
// Tipos de distribución
Normal(mean, stddev)
Discrete(probs[], labels[], n)
Uniform(min, max)
```

**API de operaciones sobre distribuciones:**

| Función | Descripción |
|---------|-------------|
| `dist_sample(d)` | Muestreo aleatorio ponderado |
| `dist_map_value(d)` | Valor más probable (Normal/Uniform) → `double` |
| `dist_map_index(d)` | Índice más probable (Discrete) → `size_t` |
| `dist_map_label(d)` | Etiqueta más probable (Discrete, con labels) → `const char*` |
| `dist_first_index(d)` | Primer índice con prob > 0 |
| `dist_complement(d)` | Distribución complementada: `P(1-d)` |
| `dist_intersect(a, b)` | Intersección: `P·P'` |
| `dist_union(a, b)` | Unión: `P+P'` |

### Colapso: map / sample / first

El colapso reduce un KaruByte multiestado a un valor determinista (`0` o `1`):

| Modo | Función | Determinista? | Descripción |
|------|---------|:---:|-------------|
| **map** | `measure:map(x)` | Sí | Máximo a Posteriori — elige el estado más probable |
| **sample** | `measure:sample(x)` | No | Muestreo ponderado aleatorio |
| **first** | `measure:first(x)` | Sí | Primera rama válida por orden de declaración |

**Colapso propagante:** si `x` colapsa y `y` depende de `x`, `y` colapsa automáticamente en cascada a través del DAG de dependencias.

---

## Lenguaje QA

### Tipos

```qa
// Tipos clásicos (deterministas)
int     x = 42;
float   y = 3.14;
string  s = "hola";
bool    b = true;

// Tipo multiestado (KaruByte)
karu    k = superposition(0, 1);
```

### Declaraciones y control de flujo

```qa
// Variables
int x = 5;
x = x + 1;

// Álgebra KaruByte
karu a = superposition(0, 1);
karu b = a AND 1;     // Álgebra multiestado nativa
karu c = NOT a;
karu d = a OR b;

// Colapso
int resultado = measure:map(a);     // determinista, el más probable
int muestra   = measure:sample(a);  // aleatorio ponderado
int primero   = measure:first(a);   // primera rama

// Condicionales
if (x > 0) {
    print(x);
} else {
    print(0);
}

// Bucles
while (i > 0) {
    print(i);
    i = i - 1;
}

// Funciones
fn doble(int x) -> int {
    return x * 2;
}
print(doble(21));
```

### Distribuciones en QA

```qa
// Normal
karu t = P(Normal(20.0, 2.5));

// Discreta con etiquetas
karu d = P(Discrete([0.7, 0.3], ["comprar", "esperar"]));

// Discreta sin etiquetas
karu e = P(Discrete([0.2, 0.8]));

// Uniforme
karu u = P(Uniform(0.0, 100.0));
```

### Ejemplo completo

```qa
// demo.qa — programa de demostración
int respuesta = 42;
string nombre = "Quanti";
print(nombre);   // → Quanti
print(respuesta); // → 42

// Superposición
karu x = superposition(0, 1);
print(x);  // → K (superposition)

// Álgebra
karu y = x AND 1;
print(measure:map(y));  // → 1

// Distribución probabilística con etiquetas
karu decision = P(Discrete([0.7, 0.3], ["comprar", "esperar"]));
print(measure:map(decision));  // → comprar (etiqueta del índice más probable)

// Control de flujo
int n = 10;
if (n > 5) {
    print(n);
}

// Funciones
fn doble(int x) -> int {
    return x * 2;
}
print(doble(21));  // → 42
```

---

## Arquitectura de la Biblioteca C

Hexagonal micromodular: el dominio multiestado está aislado; los adaptadores (CLI, frontend QA, VM/codegen) dependen de application ports, nunca al revés.

```
┌─────────────────────────────────────────────────┐
│              adapters/cli (main)                 │
├──────────────────┬──────────────────────────────┤
│ adapters/frontend│     adapters/backend         │
│ lexer parser AST │     VM · AOT codegen         │
│ interpreter      │                              │
├──────────────────┴──────────────────────────────┤
│              application (ports)                 │
│     runtime · IR · typecheck · specialize        │
├─────────────────────────────────────────────────┤
│                   domain                         │
│ KaruByte · Dist · SC · Memory · Collapse · Branch│
└─────────────────────────────────────────────────┘
```

### Módulos

| Capa | Módulo | Responsabilidad |
|------|--------|-----------------|
| **domain** | `karubyte` | 5 estados, álgebra AND/OR/NOT |
| **domain** | `distribution` | Normal, Discrete, Uniform |
| **domain** | `stochastic` | Bitstream SC backend |
| **domain** | `memory` | DAG de dependencias |
| **domain** | `collapse` | MAP/SAMPLE/FIRST + propagación |
| **domain** | `branch` / `pruner` | Fork, merge, poda |
| **application** | `runtime` | Orquestador / port principal |
| **application** | `ir` / `typecheck` / `specialize` | Pipeline de compilación |
| **adapters/frontend** | lexer→interpreter | Entrada QA |
| **adapters/backend** | `vm` / `codegen` | Ejecución IR y binario nativo |
| **adapters/cli** | `main` | CLI `run` / `vm` / `build` / `ir` |

### DAG de dependencias

Cada operación algebraica (`AND`, `OR`, `NOT`) registra automáticamente las dependencias entre nodos:

```c
KaruByte a = karu_super();           // ID: 1
KaruMemory *mem = kmem_create(64);
kmem_register(mem, a);

KaruByte b = karu_and(karu_clone(a), karu_true());  // b depende de a (ID: 2)
kmem_register(mem, b);
kmem_add_dependency(mem, b.id, a.id);  // b.id depende de a.id
```

Esto permite que al colapsar `a`, todos sus dependientes colapsen en cascada automáticamente.

### Colapso propagante

```c
collapse_propagate(mem, a.id, COLLAPSE_MAP);
// a colapsa → el DAG encuentra que b depende de a → b colapsa también
// Nodos @persistent no se ven afectados por la propagación
```

### Branching y merge

Cuando un KaruByte multiestado entra en un condicional, el runtime puede forkear ramas:

```c
KaruByte source = karu_prob(dist_discrete(probs, labels, 3));
branch_fork(mgr, source);
// 3 ramas: cada una con peso = prob[i]

// Cada rama puede modificar su estado local independientemente
branch_set_local(mgr, branch_id, karu_id, value);

// Al converger, se fusionan
KaruByte merged = branch_merge(mgr, karu_id);
// Si las ramas tienen valores distintos → nueva superposición
```

---

## Experimento de Cuantización de LLMs

### Filosofía: late collapse

El experimento aplica la filosofía Quanti a un problema real de deep learning: la cuantización de pesos de modelos de lenguaje.

**RTN (Round-To-Nearest)** cuantiza cada peso al entero más cercano — equivalente a `measure:map`. Es **determinista** y **sesgado** (cada peso tiene error de redondeo sistemático).

**Stochastic rounding** cuantiza probabilísticamente: con probabilidad = parte fraccionaria redondea hacia arriba, si no hacia abajo. Es **insesgado** (E[q] = w), pero cada muestra individual tiene más varianza que RTN.

La clave está en el **ensemble**: promediando K muestras estocásticas en el espacio de **logits** (no de pesos), el ruido se cancela y el resultado converge al modelo FP32 original. Esto es conceptualmente idéntico a mantener la incertidumbre sin colapsar hasta el final ("late collapse").

### Uso

```bash
# Requisitos
pip install torch transformers accelerate

# Full run (168 capas, K=4, per-channel)
python main.py --model Qwen/Qwen2.5-0.5B --samples 4 --seeds 5

# Cuantización selectiva: 33% de las capas (alternadas)
python main.py --model Qwen/Qwen2.5-0.5B --samples 8 --seeds 5 \
    --quantize-fraction 0.33 --layer-selection alternating

# Cuantización selectiva + per-group
python main.py --model Qwen/Qwen2.5-0.5B --samples 8 --seeds 5 \
    --quantize-fraction 0.33 --layer-selection alternating \
    --group-size 128
```

### Argumentos

| Argumento | Default | Descripción |
|-----------|---------|-------------|
| `--model` | `Qwen/Qwen2.5-0.5B` | Modelo HuggingFace |
| `--samples` | `8` | K muestras estocásticas para el ensemble |
| `--seeds` | `5` | Corridas de 1-sample para medir varianza |
| `--device` | auto | `cuda` o `cpu` |
| `--quantize-fraction` | `1.0` | Fracción de capas a cuantizar (0.0–1.0) |
| `--layer-selection` | `alternating` | `first` / `last` / `alternating` / `random` |
| `--group-size` | `0` | Tamaño de grupo per-group quant (0 = per-channel) |

### Resultados

#### Full quantization (168/168 capas, per-channel, ensemble K=4)

| Método | Perplexity | vs FP32 |
|--------|:----------:|:-------:|
| FP32 (sin cuantizar) | 19.36 | — |
| RTN-INT4 (map) | 46.64 | +140.9% |
| Stochastic-INT4 (1 muestra) | 96.08 | +396.3% |
| **Stochastic-INT4 (ensemble K=4)** | **55.70** | +187.7% |

#### Quantización selectiva: 33% de capas (alternating, per-channel, K=4)

| Método | Perplexity | vs FP32 |
|--------|:----------:|:-------:|
| FP32 (sin cuantizar) | 19.36 | — |
| RTN-INT4 (map) | 28.35 | +46.4% |
| Stochastic-INT4 (1 muestra) | 38.77 | +100.3% |
| **Stochastic-INT4 (ensemble K=4)** | **20.58** | **+6.3%** ← **supera a RTN en 27.4%** |

#### Mejor config: 33%, alternating, group-size=128, K=8

| Método | Perplexity | vs FP32 |
|--------|:----------:|:-------:|
| FP32 (sin cuantizar) | 19.36 | — |
| RTN-INT4 (map) | 27.29 | +41.0% |
| **Stochastic-INT4 (ensemble K=8)** | **20.60** | **+6.4%** ← **supera a RTN en 24.5%** |

#### Crossover: Ensemble vs RTN por fracción de capas

| Fracción | RTN | Ensemble K=4 | Resultado |
|:--------:|:---:|:------------:|:---------:|
| 100% | 46.64 | 55.70 | RTN gana |
| 50% | 29.51 | 29.52 | Empate |
| **48%** | **28.36** | **28.48** | **Crossover** |
| 45% | 27.77 | 27.53 | Ensemble gana |
| 40% | 27.78 | 25.02 | Ensemble gana −9.9% |
| 33% | 28.35 | 20.58 | Ensemble gana −27.4% |
| 25% | 27.04 | 21.47 | Ensemble gana −20.6% |

**Hallazgos clave:**

1. **Crossover en ~46%**: el ensemble estocástico supera a RTN cuando se cuantiza menos del ~46% de las capas. Más allá, la composición no lineal de muchas capas cuantizadas destruye la ventaja del promediado insesgado.
2. **Group-size ayuda a ambos**: group_size=128 reduce el MSE a la mitad y mejora más al ensemble por su mayor ruido base.
3. **Más muestras K mejoran**: con K=8 el ensemble alcanza 20.60 de perplexity (solo +6.4% de FP32). Con K→∞ converge al FP32 original por ser insesgado.
4. **Por qué funciona**: el stochastic rounding es insesgado por peso, RTN tiene sesgo sistemático. Con pocas capas cuantizadas, el promedio de logits sobre K muestras anula el ruido. Con muchas, la no-linealidad lo amplifica antes de que el promedio lo cancele.

---

## Test Suite

### Tests unitarios C

```bash
# Compilar y ejecutar todas las suites (runtime + lexer/parser/interpreter/IR)
make test
```

Suites: `test_karubyte`, `test_memory`, `test_collapse`, `test_branch`, `test_runtime`,
`test_stochastic`, `test_lexer`, `test_parser`, `test_interpreter`, `test_ir`.

### Experimento LLM (opcional)

El experimento de cuantización de LLMs documentado históricamente vivía en `main.py`.
No forma parte del runtime C actual; la hipótesis de late-collapse sigue válida a nivel conceptual.

#### test_karubyte (52 tests)

- Creación y estados (`karu_false`, `karu_true`, `karu_super`, `karu_undef`, `karu_prob`)
- Álgebra AND completa (11 casos: 0×0, 0×1, 1×1, 1×K, K×K, 0×K, Ø×1, 0×Ø, K×Ø, 1×P, P×P)
- Álgebra OR completa (9 casos)
- Álgebra NOT completa (5 casos)
- Distribuciones: creación, validación, `dist_map_index`, `dist_map_label` (con y sin etiquetas), `dist_map_value`, clonación, complemento
- Conmutatividad AND/OR (12 casos)

#### test_memory (24 tests)

- Creación, registro de nodos, búsqueda por ID
- Dependencias: agregar, conteo de aristas, dependientes directos
- Cascada transitiva: closure completo desde un nodo
- Tracking de colapso: resolución de dependencias
- Crecimiento dinámico (más nodos que capacidad inicial)

#### test_collapse (17 tests)

- Colapso individual: MAP, FIRST sobre estados 0/1/K/Ø/P
- MAP determinista: 100 colapsos de K → mismo resultado, 100 colapsos de P → mismo resultado
- Propagación: colapso en cascada a través de 3 nodos
- Respeto de `@persistent`
- `collapse_all_non_persistent`
- `collapse_mode_name`

#### test_branch (19 tests)

- Fork de K: 2 ramas, valores FALSE/TRUE, pesos 0.5
- Fork de P: pesos derivados de distribución
- Respeto de `max_branches`
- Variables locales por rama: escritura, lectura, sobrescritura
- Merge: valores divergentes → K, iguales → determinista
- Poda por umbral: estática y dinámica
- Fork de estados deterministas (FALSE, TRUE, Ø) → 0 ramas

#### test_runtime (14 tests)

- Lifecycle: init/destroy
- Creación de KaruBytes en el runtime: false, superposition, prob
- Operaciones con registro de dependencias: and, or, not
- Colapso end-to-end: measure:map, propagación a dependientes
- Branching: fork + merge cycle, prune
- Escenario de spec: `karu x = superposition(0,1); y = x AND 1`
- Escenario de IA agéntica: múltiples hipótesis con colapso tardío

### Tests end-to-end QA (19 escenarios)

El archivo `test_labels_e2e.qa` cubre:

| # | Escenario | Expected |
|---|-----------|----------|
| 1 | measure:map con labels, best middle | `b` |
| 2 | measure:map con labels, best first | `yes` |
| 3 | measure:map con labels, best last | `z` |
| 4 | measure:map con labels, equal probs | `first` |
| 5 | measure:map sin labels, best first | `0` |
| 6 | measure:map sin labels, best last | `1` |
| 7 | measure:map sobre Normal(0,1) | `0` |
| 8 | measure:map sobre Normal(1,1) | `1` |
| 9 | measure:map sobre Uniform(0,1) | `0` |
| 10 | measure:map sobre Uniform(1,3) | `1` |
| 11 | measure:map sobre superposición K | `1` |
| 12 | measure:first sobre discrete con labels | `0` |
| 13 | Álgebra AND entre discretas con labels | `0` |
| 14 | measure:map con 4 labels, best última | `z` |
| 15 | Single-element discrete con label | `only` |
| 16 | measure:map 20 veces → sin memory leak | `b` ×20 |
| 17 | measure:map asignado a string | `hello` |
| 18 | measure:sample sobre discrete con labels | `0` |
| 19 | measure:first sobre discrete con labels | `0` |

---

## Estructura del Proyecto

```
quanti/
├── domain/                   ← hexagon core (KaruByte, dist, SC, DAG, collapse, branch, pruner)
├── application/              ← ports / use-cases (runtime, IR, typecheck, specialize)
├── adapters/
│   ├── cli/                  ← driving: quanti CLI
│   ├── frontend/             ← driving: lexer, parser, AST, interpreter
│   └── backend/              ← driven: VM, AOT codegen
├── tests/
├── examples/
├── build/                    ← binaries + libquanti.a + *_gen.c
├── docs/
├── Makefile
├── README.md
└── quanti.qa.md
```

Dependency rule: `adapters → application → domain`. Domain never depends on adapters.

---

## Build

### Windows (MinGW)

```bash
# Todo
gcc -Wall -Wextra -std=c11 -I include -o build/quanti.exe ^
    src/main.c src/ast.c src/karubyte.c src/distribution.c ^
    src/memory.c src/collapse.c src/branch.c src/pruner.c ^
    src/runtime.c src/lexer.c src/parser.c src/interpreter.c -lm

# Tests
gcc -Wall -Wextra -std=c11 -I include -o build/test_karubyte.exe ^
    tests/test_karubyte.c src/karubyte.c src/distribution.c -lm
```

### Linux / macOS

```bash
# Mismos comandos, cambiar .exe → sin extensión y ^ → \
gcc -Wall -Wextra -std=c11 -I include -o build/quanti \
    src/main.c src/ast.c src/karubyte.c src/distribution.c \
    src/memory.c src/collapse.c src/branch.c src/pruner.c \
    src/runtime.c src/lexer.c src/parser.c src/interpreter.c -lm
```

### Python (experimento)

```bash
pip install torch transformers accelerate
python main.py --model Qwen/Qwen2.5-0.5B --samples 8 --seeds 5 \
    --quantize-fraction 0.33 --layer-selection alternating \
    --group-size 128
```

---

## Licencia

Quanti es un proyecto de investigación en etapa activa. Toda contribución es bienvenida.

Para más detalles sobre la especificación formal del lenguaje QA y la teoría del KaruByte, ver [`quanti.qa.md`](quanti.qa.md).
