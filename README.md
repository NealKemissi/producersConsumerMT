# producersConsumerMT

Multi-threading producers-consumer project

## Compiler le programme

Depuis le terminal.

```bash
gcc edge-detect.c bitmap.c -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all -lpthread
```
Ou
```bash
gcc edge-detect.c bitmap.c -O2 -ftree-vectorize -mavx2 -pthread -o apply-effect
```

## Lancer le programme

Depuis le terminal, lancer l'executable généré.

```bash
./a.out 
	-O2 
	-ftree-vectorize 
	-fopt-info 
	-mavx2 
	-fopt-info-vec-all 
	/home/neal/Documents/projetgcc/producersConsumerMT/input/ 
	/home/neal/Documents/projetgcc/producersConsumerMT/output/ 
	-boxblur -edgedetect -sharpen -lpthread
```