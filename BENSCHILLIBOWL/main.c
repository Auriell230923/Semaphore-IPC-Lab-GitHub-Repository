
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "BENSCHILLIBOWL.h"

#define BENSCHILLIBOWL_SIZE 100
#define NUM_CUSTOMERS 90
#define NUM_COOKS 10
#define ORDERS_PER_CUSTOMER 3
#define EXPECTED_NUM_ORDERS (NUM_CUSTOMERS * ORDERS_PER_CUSTOMER)

/* Global restaurant */
BENSCHILLIBOWL *bcb;

/* Customer thread */
void* BENSCHILLIBOWLCustomer(void* tid) {

    int customer_id = (int)(long)tid;

    for (int i = 0; i < ORDERS_PER_CUSTOMER; i++) {

        Order* order = malloc(sizeof(Order));

        if (order == NULL) {
            perror("Failed to allocate order");
            exit(EXIT_FAILURE);
        }

        order->menu_item = PickRandomMenuItem();
        order->customer_id = customer_id;
        order->order_number = 0;
        order->next = NULL;

        /*
         * Save the menu item before submitting the order.
         * A cook may free the order after AddOrder returns.
         */
        MenuItem selected_item = order->menu_item;

        int order_number = AddOrder(bcb, order);

        printf("Customer #%d placed order #%d for %s\n",
               customer_id,
               order_number,
               selected_item);
    }

    return NULL;
}

/* Cook thread */
void* BENSCHILLIBOWLCook(void* tid) {

    int cook_id = (int)(long)tid;
    int orders_fulfilled = 0;

    while (1) {

        Order* order = GetOrder(bcb);

        if (order == NULL) {
            break;
        }

        printf("Cook #%d fulfilled order #%d for "
               "Customer #%d: %s\n",
               cook_id,
               order->order_number,
               order->customer_id,
               order->menu_item);

        orders_fulfilled++;

        free(order);
    }

    printf("Cook #%d fulfilled %d orders\n",
           cook_id, orders_fulfilled);

    return NULL;
}

/* Main function */
int main() {

    pthread_t customers[NUM_CUSTOMERS];
    pthread_t cooks[NUM_COOKS];

    srand((unsigned int)time(NULL));

    /* Open restaurant */
    bcb = OpenRestaurant(BENSCHILLIBOWL_SIZE,
                         EXPECTED_NUM_ORDERS);

    /* Create cook threads */
    for (int i = 0; i < NUM_COOKS; i++) {

        if (pthread_create(&cooks[i], NULL,
                           BENSCHILLIBOWLCook,
                           (void*)(long)(i + 1)) != 0) {

            perror("Failed to create cook");
            exit(EXIT_FAILURE);
        }
    }

    /* Create customer threads */
    for (int i = 0; i < NUM_CUSTOMERS; i++) {

        if (pthread_create(&customers[i], NULL,
                           BENSCHILLIBOWLCustomer,
                           (void*)(long)(i + 1)) != 0) {

            perror("Failed to create customer");
            exit(EXIT_FAILURE);
        }
    }

    /* Wait for all customers */
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customers[i], NULL);
    }

    /* Wait for all cooks */
    for (int i = 0; i < NUM_COOKS; i++) {
        pthread_join(cooks[i], NULL);
    }

    /* Close restaurant */
    CloseRestaurant(bcb);

    return 0;
}
