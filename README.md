# producersConsumerMT

Multi-threading producers-consumer project

## Lancer le programme

Depuis le terminal.

```bash
gcc edge-detect.c bitmap.c -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all -lpthread
```
Ou
```bash
gcc edge-detect.c bitmap.c -O2 -ftree-vectorize -mavx2 -pthread -o apply-effect
```