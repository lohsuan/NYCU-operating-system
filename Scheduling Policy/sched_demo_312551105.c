#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int opt;
    int num_threads = atoi(argv[2]);
    float time_wait = 0;
    char *policies[num_threads];
    int priorities[num_threads];

    /* 1. Parse program arguments */

    while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
        switch (opt) {
        case 'n':
            printf("n=%s\n", optarg);
            if (atoi(optarg) != num_threads)
                exit(EXIT_FAILURE);
            break;
        case 't':
            printf("t=%s\n", optarg);
            time_wait = atof(optarg);
            break;
        case 's':
            printf("s=%s\n", optarg);
            char *s_string = optarg;
            char *s_token = strtok(s_string, ",");
            for (int i = 0; i < num_threads; i++) {
                policies[i] = s_token;
                s_token = strtok(NULL, ",");
            }
            break;
        case 'p':
            printf("p=%s\n", optarg);
            char *p_string = optarg;
            char *p_token = strtok(p_string, ",");
            for (int i = 0; i < num_threads; i++) {
                priorities[i] = atoi(p_token);
                p_token = strtok(NULL, ",");
            }
            break;
        default:
            fprintf(stderr, "input usage error\n");
            exit(EXIT_FAILURE);
        }
    }

    /* 2. Create <num_threads> worker threads */

    /* 3. Set CPU affinity */

    for (int i = 0; i < num_threads; i++) {
        /* 4. Set the attributes to each thread */
    }
    /* 5. Start all threads at once */

    /* 6. Wait for all threads to finish  */
}
