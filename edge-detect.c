/*
	//gcc edge-detect.c bitmap.c -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all
	//UTILISER UNIQUEMENT DES BMP 24bits
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bitmap.h"
#include <stdint.h>
#include <dirent.h>
#include "string.h"
#include <pthread.h>

#define DIM 3
#define LENGHT DIM
#define OFFSET DIM /2
#define STACK_MAX 10
#define NB_THREADS_MAX 2
#define INPUT_DIR "/home/neal/Documents/projetgcc/producersConsumerMT/input/"
#define OUTPUT_DIR "/home/neal/Documents/projetgcc/producersConsumerMT/output/"

const float KERNEL[DIM][DIM] = {{-1, -1,-1},
							   {-1,8,-1},
							   {-1,-1,-1}};

typedef struct Color_t {
	float Red;
	float Green;
	float Blue;
} Color_e;


/************************************ la pile partagée ************************************/
typedef struct stack_t {
	Image data[STACK_MAX]; // le tableau contient des pointeurs de l'img
	int count;
	int max;
	pthread_mutex_t lock;
	pthread_cond_t can_consume;
	pthread_cond_t can_produce;
} Stack;

static Stack stack;

void stack_init() {
	pthread_cond_init(&stack.can_produce, NULL);
	pthread_cond_init(&stack.can_consume, NULL);
	pthread_mutex_init(&stack.lock, NULL);
	stack.max = STACK_MAX;
	stack.count = 0;
	srand(time(NULL));
}

/************************************ les producteurs ************************************/

//********************************** deuxieme methode
void* producing(void* arg);
void* producing(void* arg) {
	/** pointer du repertoire d'entree **/
	struct dirent *deInput = NULL;
    DIR *dr = opendir(INPUT_DIR);
    if(dr == NULL) {
        printf("Impossible d'ouvrir le repertoire d'entree :(\n");
        return NULL;
    }

	while((deInput = readdir(dr)) != NULL) {
		pthread_mutex_lock(&stack.lock);

			if(stack.count < stack.max) {
				/** traitement de l'img **/
				Image img = open_bitmap(strcat(INPUT_DIR, deInput->d_name));
				Image new_i;
				apply_effect(&img, &new_i);
				stack.data[stack.count] = new_i;
				stack.count++;
				printf("[PRODUCER] J'ai produit !\n");
				pthread_cond_signal(&stack.can_consume);
			} else {
				printf("[PRODUCER] Je ne peux plus produire :(\n");
				while(stack.count >= stack.max) {
					pthread_cond_wait(&stack.can_produce, &stack.lock);
				}
				printf("[PRODUCER] Je peux a nouveau produire :)\n");
			}

		pthread_mutex_unlock(&stack.lock);
	}
	closedir(dr);
}

/************************************ le consommateur ************************************/
void* consumer(void* arg);
void* consumer(void* arg) {
	while(1) {
		pthread_mutex_lock(&stack.lock);
			
			while(stack.count == 0) {
				printf("[CONSUMER] Rien a consommer :( \n");
				pthread_cond_wait(&stack.can_consume, &stack.lock);
			}
			/** consommation de l'img **/
			printf("[CONSUMER] Je consomme !\n");
			save_bitmap(stack.data[stack.count], strcat(OUTPUT_DIR, "output.bmp")); 
			stack.count--;
			printf("[CONSUMER] J'ai finit, la nouvelle image est sur le disque !\n");
			pthread_cond_signal(&stack.can_produce);
		
		pthread_mutex_unlock(&stack.lock);
	}
	return 0;
}

/************************************ apply effect ************************************/
void apply_effect(Image* original, Image* new_i);
void apply_effect(Image* original, Image* new_i) {

	int w = original->bmp_header.width;
	int h = original->bmp_header.height;

	*new_i = new_image(w, h, original->bmp_header.bit_per_pixel, original->bmp_header.color_planes);

	for (int y = OFFSET; y < h - OFFSET; y++) {
		for (int x = OFFSET; x < w - OFFSET; x++) {
			Color_e c = { .Red = 0, .Green = 0, .Blue = 0};

			for(int a = 0; a < LENGHT; a++){
				for(int b = 0; b < LENGHT; b++){
					int xn = x + a - OFFSET;
					int yn = y + b - OFFSET;

					Pixel* p = &original->pixel_data[yn][xn];

					c.Red += ((float) p->r) * KERNEL[a][b];
					c.Green += ((float) p->g) * KERNEL[a][b];
					c.Blue += ((float) p->b) * KERNEL[a][b];
				}
			}

			Pixel* dest = &new_i->pixel_data[y][x];
			dest->r = (uint8_t)  (c.Red <= 0 ? 0 : c.Red >= 255 ? 255 : c.Red);
			dest->g = (uint8_t) (c.Green <= 0 ? 0 : c.Green >= 255 ? 255 : c.Green);
			dest->b = (uint8_t) (c.Blue <= 0 ? 0 : c.Blue >= 255 ? 255 : c.Blue);
		}
	}
}

/******************************************************************************/
/* 									MAIN 									  */
/******************************************************************************/
int main(int argc, char** argv) {
	// Image img = open_bitmap("bmp_tank.bmp");
	// Image new_i;
	// apply_effect(&img, &new_i);
	// save_bitmap(new_i, "test_out.bmp");

	pthread_t threads_id[5];
	stack_init();
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	/** creation des N producteurs **/
	for(int i = 0; i < 4; i++) {
		pthread_create(&threads_id[i], &attr, producing, NULL);
	}
	/** creation du consommateur **/
	pthread_create(&threads_id[4], NULL, consumer, NULL);
	/** on attends le consommateur **/
	pthread_join(threads_id[4] ,NULL);

	return 0;
}