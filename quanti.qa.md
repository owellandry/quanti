# Quanti
### Multi-State Runtime Programming — Hipótesis, Especificación y Hoja de Ruta

**Autor:** Andry Silva / Owell
**Versión:** 3.0 — Especificación Consolidada
**Estado:** Hipótesis activa en fase de formalización

---

## Qué es Quanti

Quanti es un sistema computacional experimental compuesto por:

- un **lenguaje de programación** (`.qa`) con semántica multiestado nativa,
- una **unidad lógica abstracta** llamada `KaruByte` capaz de representar múltiples estados simultáneamente,
- un **runtime** que mantiene ramas de ejecución paralelas y decide cuándo colapsarlas.

La hipótesis central es:

> Un sistema computacional clásico puede aproximar ciertos beneficios conceptuales de la computación cuántica mediante un runtime multiestado capaz de mantener y procesar simultáneamente múltiples posibilidades lógicas, utilizando estructuras simbólicas, ejecución diferida y representación probabilística.

Quanti **no** es computación cuántica. No manipula partículas. No requiere hardware especializado. Es un paradigma de software construido sobre hardware clásico, orientado a hacer de la incertidumbre una primitiva nativa del lenguaje — no una librería externa.

---

## El Problema que Resuelve

La programación moderna maneja la incertidumbre de forma torpe:

```python
# Enfoque clásico: incertidumbre como afterthought
estado = get_estado_agente()
if estado == "A":
    hipotesis_a()
elif estado == "B":
    hipotesis_b()
else:
    hipotesis_default()
```

Los sistemas que trabajan con incertidumbre real —agentes de IA, simulaciones, planificadores— necesitan mantener múltiples hipótesis activas simultáneamente, razonar sobre todas ellas, y colapsar solo cuando tienen suficiente información. Hacerlo con `if/else` y librerías probabilísticas externas es costoso cognitivamente y arquitectónicamente frágil.

Quanti propone que la incertidumbre sea un **ciudadano de primera clase** del lenguaje:

```qa
// Enfoque Quanti: incertidumbre como primitiva nativa
karu estado = superposition("A", "B", "C");

// El runtime ejecuta las tres ramas simultáneamente
// El colapso ocurre solo cuando el sistema lo requiere
when (evidencia_suficiente) {
    measure:map(estado);  // colapsa al más probable
}
```

---

## Caso de Uso Killer — IA Agéntica

El dominio inicial y principal de Quanti es la **IA agéntica**: sistemas donde un agente debe mantener múltiples hipótesis sobre el mundo, razonar sobre todas ellas, y actuar bajo incertidumbre sin colapsar prematuramente a una sola interpretación.

**Problema hoy:**

Un agente de IA que modela el estado de un usuario debe elegir una hipótesis sobre su intención antes de actuar. Si elige mal, falla. La solución actual es ejecutar múltiples modelos en paralelo con código orquestal complejo.

**Con Quanti:**

```qa
// El agente mantiene múltiples modelos del mundo simultáneamente
karu intencion_usuario = superposition(
    Intencion::Comprar,
    Intencion::Explorar,
    Intencion::Comparar
);

// Razona sobre todas las ramas sin colapsar
karu respuesta = generar_respuesta(intencion_usuario);

// Colapsa solo cuando el contexto lo exige (respuesta al usuario)
output(measure:map(respuesta));
```

El runtime mantiene las tres ramas activas, las evalúa en paralelo, y colapsa al momento de producir output. El programador no orquesta el paralelismo manualmente.

---

## Estado del Arte — Posicionamiento

Quanti no inventa lógica probabilística, ejecución simbólica, ni computación cuántica. Estos campos existen y están activos. La siguiente tabla define dónde está el aporte diferencial:

| Campo | Qué hace | Qué NO hace |
|---|---|---|
| **PPL** (Pyro, Stan, PyMC) | Inferencia bayesiana, incertidumbre como librería | No propone tipos multiestado nativos en un lenguaje de propósito general |
| **Ejecución simbólica** (KLEE, angr) | Múltiples caminos simultáneos | Orientado a testing/análisis, no a programación cotidiana |
| **Lógica difusa / MVL** | Grados de verdad formalizados | No propone un runtime ejecutable de propósito general |
| **Lazy evaluation** (Haskell) | Difiere cómputo determinista | No difiere estados probabilísticos coexistentes |
| **Computación cuántica** | Superposición física real | Requiere hardware especializado, no accesible |

**El hueco de Quanti:** un lenguaje de propósito general donde la incertidumbre es una primitiva nativa, orientado al desarrollador de software — no al estadístico, no al investigador de seguridad.

---

## KaruByte — Especificación Formal

### Definición

`KaruByte` es la unidad lógica fundamental de Quanti. Es un **valor único multiestado** — análogo conceptual a un qubit, pero implementado en software sobre lógica clásica.

Un `KaruByte` puede encontrarse en uno de cinco estados:

| Estado | Símbolo | Significado | Equivalente formal |
|---|---|---|---|
| Falso | `0` | Valor determinista falso | Booleano `false` |
| Verdadero | `1` | Valor determinista verdadero | Booleano `true` |
| Superposición | `K` | Ambos valores simultáneamente activos | Lógica trivaluada de Kleene extendida |
| Indefinido | `Ø` | Estado desconocido, no resuelto | Bottom type (⊥) en teoría de tipos |
| Probabilístico | `P(d)` | Distribución de probabilidad asociada | Variable aleatoria con distribución `d` |

`KaruByte` es un **valor único** — no un byte de 8 posiciones. El nombre es una decisión semántica, no estructural: evoca que este tipo carga más información que un bit clásico, análogo a cómo un byte carga más que un bit.

---

### Álgebra del KaruByte

#### Tabla AND

| AND | `0` | `1` | `K` | `Ø` | `P` |
|---|---|---|---|---|---|
| `0` | `0` | `0` | `0` | `0` | `0` |
| `1` | `0` | `1` | `K` | `Ø` | `P` |
| `K` | `0` | `K` | `K` | `Ø` | `K∩P` |
| `Ø` | `0` | `Ø` | `Ø` | `Ø` | `Ø` |
| `P` | `0` | `P` | `K∩P` | `Ø` | `P·P'` |

#### Tabla OR

| OR | `0` | `1` | `K` | `Ø` | `P` |
|---|---|---|---|---|---|
| `0` | `0` | `1` | `K` | `Ø` | `P` |
| `1` | `1` | `1` | `1` | `1` | `1` |
| `K` | `K` | `1` | `K` | `K∪Ø` | `K∪P` |
| `Ø` | `Ø` | `1` | `K∪Ø` | `Ø` | `Ø∪P` |
| `P` | `P` | `1` | `K∪P` | `Ø∪P` | `P+P'` |

#### NOT

| Estado | NOT |
|---|---|
| `0` | `1` |
| `1` | `0` |
| `K` | `K` (la superposición se invierte en ambas ramas) |
| `Ø` | `Ø` (lo indefinido permanece indefinido) |
| `P(d)` | `P(1-d)` (la distribución se complementa) |

#### Reglas de absorción

- `Ø AND cualquier_cosa` = `Ø` — lo indefinido absorbe en AND (no puedo afirmar sin datos)
- `1 OR cualquier_cosa` = `1` — la certeza absorbe en OR
- `0 AND cualquier_cosa` = `0` — el falso absorbe en AND

#### Propiedades

- Las operaciones son **conmutativas**: `K AND P = P AND K`
- Las operaciones son **asociativas**: `(K AND P) AND 1 = K AND (P AND 1)`
- `K` **no es absorbente** en OR — conserva ambas ramas activas

#### Estado P — Distribución Asociada

`P` no es un float simple entre 0 y 1. Es una **distribución completa** asociada al valor:

```qa
// P con distribución normal
karu temperatura = P(Normal(20.0, 2.5));

// P con distribución discreta
karu decision = P(Discrete([0.6, 0.3, 0.1], ["comprar", "esperar", "vender"]));

// P con distribución uniforme
karu posicion = P(Uniform(0.0, 100.0));
```

Esto permite que el runtime propague incertidumbre de forma matemáticamente coherente a través de operaciones.

---

## Semántica del Colapso

### Definición

El colapso es el proceso mediante el cual un `KaruByte` en estado `K`, `Ø` o `P` se reduce a un valor determinista (`0` o `1`).

### Estrategias de Colapso — Controladas por el Programador

El colapso es explícito y la estrategia es seleccionable. El runtime ofrece tres modos:

#### `measure:map(x)` — Máximo a Posteriori (default)
Selecciona el estado de mayor probabilidad. **Determinista**: mismo estado siempre produce mismo resultado.

```qa
karu x = superposition(0, 1);  // K
int resultado = measure:map(x);  // → siempre 1 (ambos igualmente probables → desempate por orden)

karu y = P(Discrete([0.7, 0.3], [0, 1]));
int r2 = measure:map(y);  // → siempre 0 (0.7 > 0.3)
```

#### `measure:sample(x)` — Muestreo Ponderado
Selecciona aleatoriamente según la distribución de probabilidad. **No determinista**: mismo estado puede producir resultados distintos.

```qa
karu y = P(Discrete([0.7, 0.3], [0, 1]));
int r = measure:sample(y);  // → 0 con 70% de probabilidad, 1 con 30%
```

#### `measure:first(x)` — Primera Rama Válida
Selecciona la primera rama activa en el orden de declaración. **Determinista por orden de inserción.**

```qa
karu x = superposition(0, 1);
int r = measure:first(x);  // → siempre 0 (primera rama)
```

### Default

El modo por defecto es `measure:map` — determinista, predecible, sin sorpresas.

### Colapso Propagante

Si `x` colapsa y `y` depende de `x`, `y` colapsa automáticamente:

```qa
karu x = superposition(0, 1);
karu y = x AND 1;  // y depende de x

measure:map(x);  // x colapsa → y colapsa en cascada
```

El grafo de dependencias es mantenido por el runtime (KaruMemory). El colapso propaga por el DAG de dependencias.

### Anti-colapso Explícito

El default del runtime es **mantener en superposición** hasta que se fuerce colapso. El programador puede marcar explícitamente que una variable debe resistir colapso automático:

```qa
karu x = superposition(0, 1) @persistent;  // no colapsa aunque el runtime presione por memoria
```

---

## Branching Runtime

### Modelo de Memoria

Cada rama tiene su **propia copia del estado local** (fork semántico), pero comparte estructuras de solo lectura con las demás ramas mediante el grafo KaruMemory. Esto evita duplicar datos inmutables.

```
Estado global compartido (solo lectura)
         │
    ┌────┴────┐
  Rama A    Rama B    ← copias del estado mutable
  (x=0)     (x=1)
```

### Fusión de Ramas

Cuando dos ramas convergen con valores distintos para la misma variable, el runtime crea automáticamente un nuevo `superposition`:

```qa
karu x = superposition(0, 1);

// Rama A: x=0 → produce resultado=3
// Rama B: x=1 → produce resultado=7

// Al converger:
// resultado = superposition(3, 7)  ← fusión automática
```

### Prioridad y Peso de Ramas

Las ramas pueden tener peso asociado, derivado de la distribución del `KaruByte` que las originó:

```qa
karu x = P(Discrete([0.8, 0.2], [0, 1]));
// Rama A tiene peso 0.8
// Rama B tiene peso 0.2
// measure:map siempre elegirá Rama A
```

### Límite de Ramas y Poda

El runtime aplica **poda adaptativa** automática:

- Ramas con peso < umbral configurable son descartadas
- Cuando la presión de memoria excede el límite, el runtime colapsa las ramas de menor peso
- El programador puede configurar los umbrales:

```qa
@runtime(max_branches: 64, prune_threshold: 0.01)
```

---

## Tipos y Lenguaje QA

### Sistema de Tipos

QA soporta dos clases de tipos:

**Tipos clásicos (deterministas):**
```qa
int, float, string, bool, array<T>, struct
```

**Tipos multiestado (KaruByte):**
```qa
karu, karu<int>, karu<string>, karu<T>
```

Los tipos clásicos no pasan por el runtime multiestado. Son exactamente equivalentes a tipos en cualquier lenguaje clásico. El programador elige cuándo necesita incertidumbre nativa.

### Arrays Multiestado

Un array puede contener elementos en superposición independiente:

```qa
karu[] mundo = [superposition(1, 2), superposition(3, 4)];
// Esto crea 4 ramas posibles: [1,3], [1,4], [2,3], [2,4]
// El runtime mantiene los 4 mundos activos
```

### Paradigma del Lenguaje

QA es **híbrido imperativo-funcional**:

- Imperativo para control de flujo y efectos
- Funcional para transformaciones de estados multiestado (operaciones sobre `karu` son puras)

```qa
// Imperativo
int x = 5;
x = x + 1;

// Funcional sobre multiestado
karu resultado = map(superposition(1, 2, 3), fn(x) => x * 2);
// resultado = superposition(2, 4, 6)
```

### Efectos Secundarios y I/O

Los efectos secundarios (I/O, red, disco) **fuerzan colapso automático** con `measure:map`:

```qa
karu nombre = superposition("Alice", "Bob");

// Esto fuerza colapso automático antes de imprimir
print(nombre);  // → imprime "Alice" (primera rama, map default)

// Para control explícito:
print(measure:sample(nombre));  // → "Alice" o "Bob" aleatoriamente
```

Esta regla garantiza que el mundo externo siempre recibe valores deterministas.

### Deployment

QA es un **lenguaje standalone con su propio runtime**. No es un DSL embebido. Tiene parser, intérprete tree-walk, IR, VM bytecode y compilador AOT (`quanti build`) que genera C y linkea `libquanti` hacia un binario nativo. La implementación de referencia es **C11**.

```bash
quanti examples/demo.qa              # intérprete
quanti vm examples/demo.qa           # IR + VM
quanti build examples/demo.qa -o build/demo.exe --lto   # AOT nativo
```

---

## Arquitectura del Sistema

```
┌──────────────────────────────────────────────────────────┐
│                    QA Language (.qa)                     │
│         Lenguaje híbrido imperativo-funcional            │
│         con tipos karu nativos                          │
└─────────────────────────┬────────────────────────────────┘
                          │ parse
┌─────────────────────────▼────────────────────────────────┐
│                    Quanti Runtime                        │
│                                                          │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ Branch Mgr  │  │ KaruMemory   │  │ Collapse Engine│  │
│  │             │  │              │  │                │  │
│  │ Crea, pesa  │  │ DAG shared   │  │ MAP / SAMPLE / │  │
│  │ y poda      │  │ entre ramas  │  │ FIRST          │  │
│  │ ramas       │  │ (solo lect.) │  │ + propagación  │  │
│  └─────────────┘  └──────────────┘  └────────────────┘  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │ Adaptive Pruner                                  │   │
│  │ Descarta ramas con peso < umbral configurable    │   │
│  └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

---

## Diferencias con Computación Cuántica

| Quanti | Computación Cuántica Real |
|---|---|
| Emulación lógica por software | Fenómeno físico real |
| Hardware clásico estándar | Qubits, criogenia, entornos controlados |
| Superposición semántica | Superposición física |
| Accesible hoy con cualquier computadora | Hardware de cientos de miles de dólares |
| Lenguaje de propósito general | Circuitos cuánticos especializados |
| Probabilístico/simbólico | Mecánica cuántica real |

**Nota de posicionamiento:** Quanti no debe comercializarse como "cuántico". El framing correcto es **Multi-State Runtime Programming** o **Probabilistic Native Runtime**. La analogía cuántica es útil pedagógicamente, pero genera expectativas incorrectas y escepticismo académico innecesario.

---

## El Problema Central — Explosión de Estados

Cada `superposition` incrementa exponencialmente el número de estados activos:

```
1 karu  →  2 estados
2 karus →  4 estados
3 karus →  8 estados
n karus →  2ⁿ estados
```

Este es el mismo problema del **path explosion** en ejecución simbólica — uno de los problemas abiertos más estudiados en ciencias de la computación. No tiene solución general. Quanti lo ataca con cinco estrategias combinadas:

| Estrategia | Descripción | Equivalente académico |
|---|---|---|
| **Lazy Branching** | Crear ramas solo cuando sean necesarias | On-demand path exploration (KLEE) |
| **State Merging** | Fusionar estados equivalentes | State merging heuristics |
| **Shared DAGs** | Compartir datos entre ramas por referencia | Binary Decision Diagrams (BDDs) |
| **Probabilistic Pruning** | Eliminar ramas con peso < umbral | Importance sampling / beam search |
| **Adaptive Collapse** | Colapsar automáticamente bajo presión de memoria | Early termination en búsqueda heurística |

**El éxito de Quanti depende de un único factor:** encontrar dominios donde `beneficio_contextual > costo_computacional`. La IA agéntica es la apuesta inicial porque mantener múltiples hipótesis activas tiene valor de negocio directo y medible.

---

## Riesgos

| Riesgo | Severidad | Mitigación |
|---|---|---|
| Explosión exponencial sin solución general | Alta | Restricción de dominio + pruning agresivo |
| Semántica del colapso mal definida | Alta | Formalización matemática antes de implementar |
| Reinvención sin diferenciación vs. PPL | Media | Posicionamiento en lenguaje general, no herramienta estadística |
| Costo de memoria prohibitivo en casos reales | Media | Benchmark temprano para validar viabilidad |
| Adopción — curva de aprendizaje alta | Media | DSL mínimo primero, no feature-complete |

---

## Estado Actual del Proyecto

| Componente | Estado |
|---|---|
| Hipótesis conceptual | Completa |
| Posicionamiento en estado del arte | Completa |
| Álgebra KaruByte | Especificada e implementada en C |
| Semántica del colapso | Especificada e implementada |
| Spec formal del lenguaje QA | Parcial — paradigma, tipos, when/@runtime |
| Intérprete tree-walk (C11) | Completo |
| Branching en QA (`if`/`when` sobre `karu`) | Completo |
| Quenti IR + bytecode VM | Completo |
| AOT a binario nativo (`quanti build`) | Completo (IR→C→gcc + libquanti) |
| Especialización de regiones clásicas + LTO | Completo |
| Benchmark vs. alternativas externas | Pendiente |
| Paper o publicación | Pendiente |

---

## Hoja de Ruta

### Fase 1 — Formalización (actual)
- Álgebra completa del KaruByte ✅
- Semántica operacional del colapso ✅
- Estudio profundo de: Pyro, PyMC, KLEE, lógica de Kleene

### Fase 2 — Prototipo Mínimo
- Intérprete de QA en Python que ejecute:
```qa
karu x = superposition(0, 1);
karu y = x AND 1;
print(measure:map(y));   // → 1
print(measure:first(y)); // → 0
```
- Demostración de branching básico con 2-3 variables
- Primer benchmark comparativo contra Python + Pyro

### Fase 3 — Caso de Uso Real
- Implementar un agente de IA simple con múltiples hipótesis sobre el estado del usuario
- Comparar complejidad del código Quanti vs. implementación clásica equivalente
- Validar que el modelo es expresivamente superior en este dominio

### Fase 4 — Publicación
- Spec técnica formal del lenguaje QA
- Paper describiendo el modelo de ejecución y resultados del benchmark
- Runtime open source con documentación

---

## Referencias Académicas

- Bingham et al. (2019). *Pyro: Deep Universal Probabilistic Programming.* arXiv:1810.09538
- van de Meent et al. (2018). *An Introduction to Probabilistic Programming.* arXiv:1809.10756
- Baldoni et al. (2018). *A Survey of Symbolic Execution Techniques.* ACM Computing Surveys, Vol. 51, No. 3.
- Schemmel et al. (2020). *Symbolic Partial-Order Execution for Testing Multi-Threaded Programs.*
- Bhattacharjee et al. (2018). *Multi-valued and Fuzzy Logic Realization using TaOx Memristive Devices.* Nature Scientific Reports. DOI: 10.1038/s41598-017-18329-3
- Baudart et al. *Compiling Stan to Generative Probabilistic Languages.* arXiv:1810.00873
- Garg et al. (2024). *Bit Blasting Probabilistic Programs.* Proc. ACM Program. Lang., 8, PLDI.
- Cambridge University Press. *Practical Foundations for Programming Languages* — Chapter 36: Lazy Evaluation.

---

*Quanti es un proyecto de investigación en etapa de hipótesis activa. Toda contribución, crítica o colaboración es bienvenida.*