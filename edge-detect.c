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
//#define INPUT_DIR "/home/neal/Documents/projetgcc/producersConsumerMT/input/"
//#define OUTPUT_DIR "/home/neal/Documents/projetgcc/producersConsumerMT/output/"
#define INDEX_INPUT_DIR 6
#define INDEX_OUTPUT_DIR 7

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
	char* names_files[STACK_MAX];
	int count_in;
	int count;
	int files_treated;
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

/************************************ utilitaires manipulations fichiers ************************************/
char *get_input_file(char *const *argv, char *ch);
char *get_input_file(char *const *argv, char *ch) {
	const unsigned long inputDirLength = strlen(argv[INDEX_INPUT_DIR]) + strlen(ch);
    char *inputFile = malloc(sizeof(char) * (inputDirLength));
    if (NULL == inputFile) {
        printf(stderr, "[PRODUCER] Allocation impossible :(\n");
    }
    inputFile = strcat(inputFile, argv[INDEX_INPUT_DIR]);
    inputFile = strcat(inputFile, ch);

    return inputFile;
}
char *get_output_file(char *const *argv, char *ch);
char *get_output_file(char *const *argv, char *ch) {
    const unsigned long outPutDirLength = strlen(argv[INDEX_OUTPUT_DIR]) + strlen(ch);
    char *outputFile = malloc(sizeof(char) * (outPutDirLength));
    if (NULL == outputFile) {
        printf(stderr, "[CONSUMER] Allocation impossible :(\n");
    }
    outputFile = strcat(outputFile, argv[INDEX_OUTPUT_DIR]);
    outputFile = strcat(outputFile, ch);

    return outputFile;
}
void *get_all_files(void* arg);
void *get_all_files(void* arg) {
	char *const *argv = (char *const *) arg;
	/** pointer du repertoire d'entree **/
	struct dirent *deInput = NULL;
    DIR *dr = opendir(argv[INDEX_INPUT_DIR]);
    if(dr == NULL) {
        printf("[INFO] Impossible d'ouvrir le repertoire d'entree.\n");
        return -1;
    }
    /** on recup tout les noms d'img pour la suite (uniquement les .bmp) **/
    while((deInput = readdir(dr)) != NULL) {
    	if (strstr(deInput->d_name, ".bmp") != NULL) {
    		const unsigned long length = strlen(argv[INDEX_INPUT_DIR]) + strlen(deInput->d_name);
    		char *input = malloc(sizeof(char) * (length));
    		if (input == NULL) {
        		printf(stderr, "[INFO] Allocation impossible :(\n");
    		}
    		input = strcat(input, argv[INDEX_INPUT_DIR]);
    		input = strcat(input, deInput->d_name);

    		printf("[INFO] %s\n", input);
    		stack.names_files[stack.count_in] = input;
        	stack.count_in ++;
		}
    }
    closedir(dr);
    printf("[INFO]\n");
    printf("[INFO] Nombre  Total d'image a traiter : %d\n", stack.count_in);
    return 0;
}

/************************************ les producteurs ************************************/
void* producing(void* arg);
void* producing(void* arg) {
	char *const *argv = (char *const *) arg;

	while(1) {
		pthread_mutex_lock(&stack.lock);

			if(stack.count < stack.max) {
				/** traitement de l'img **/
				printf("[PRODUCER] Je produit.....(%s)\n", stack.names_files[stack.count]);
				Image img = open_bitmap(stack.names_files[stack.count]);
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
	return 0;
}

/************************************ le consommateur ************************************/
void* consumer(void* arg);
void* consumer(void* arg) {
	char *const *argv = (char *const *) arg;
	int cpt = 0;

	while(1) {
		pthread_mutex_lock(&stack.lock);
			
			while(stack.count == 0) {
				printf("[CONSUMER] Rien a consommer :( \n");
				pthread_cond_wait(&stack.can_consume, &stack.lock);
			}
			stack.count--;
			cpt++;
			/** consommation de l'img **/
			char filename[20];
			sprintf(filename, "output%d.bmp", cpt);
			const unsigned long length = strlen(argv[INDEX_OUTPUT_DIR]) + strlen(filename);
    		char *output = malloc(sizeof(char) * (length));
    		if (output == NULL) {
        		printf(stderr, "[CONSUMER] Allocation impossible :(\n");
    		}
    		output = strcat(output, argv[INDEX_OUTPUT_DIR]);
    		output = strcat(output, filename);


			printf("[CONSUMER] Je consomme !\n");
			printf("[CONSUMER] Path : %s\n", output);
			save_bitmap(stack.data[stack.count], output); 
			printf("[CONSUMER] J'ai finit, la nouvelle image est sur le disque !\n");
			output = NULL;
			free(output);
			if(cpt >= 11) {
                printf("[CONSUMER] Les %d images ont toutes ete sauvegardees !\n", 11);
                break;
            }
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
	printf("[INFO] ----------------------------------------------------------------\n");
	printf("[INFO] Lancement du programme ...\n");
	printf("[INFO] ----------------------------------------------------------------\n");
	printf("[INFO]\n");
	printf("[INFO] Dossier d'entree : %s\n", argv[INDEX_INPUT_DIR]);
	printf("[INFO] Dossier de sortie : %s\n", argv[INDEX_OUTPUT_DIR]);
	printf("[INFO]\n");
	printf("[INFO] ----------------------------------------------------------------\n");
	printf("[INFO]\n");
	get_all_files((void*) argv);
	printf("[INFO]\n");
	printf("[INFO] ----------------------------------------------------------------\n");

	pthread_t threads_id[5];
	stack_init();
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	/** creation des N producteurs **/
	for(int i = 0; i < 4; i++) {
		pthread_create(&threads_id[i], &attr, producing, (void*) argv); // ON DONNE EN ARGS LE DOSSIER INPUT
	}
	/** creation du consommateur **/
	pthread_create(&threads_id[4], NULL, consumer, (void*) argv);
	/** on attends le consommateur **/
	pthread_join(threads_id[4] ,NULL);

	printf("[INFO]\n");
	printf("[INFO] ----------------------------------------------------------------\n");
	printf("[INFO] Fin du programme\n");
	printf("[INFO] ----------------------------------------------------------------\n");
	return 0;
}