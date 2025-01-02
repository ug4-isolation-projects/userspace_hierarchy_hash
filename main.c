#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

int main()
{
    printf("Testbench for userspace hierarchical hash implementation.\n");
    printf("Please choose one of the following commands:\n");
    printf("1. \"add\": Add a new value to the hash table, provided a theoretical uid and a value.\n");
    printf("   Will be hashed to a bucket based on the addr and the uid.\n");
    printf("2. \"remove\": Remove a value from the hash table, given a theoretical uid and a value.\n");
    printf("   If this was the only value in a bucket, the bucket should be destroyed.\n");
    printf("3. \"print\": Print the contents of a bucket (i.e its sub buckets and their contents).\n");
    printf("4. \"resize\": Force add many values to a bucket to test resizing.\n");
    printf("5. \"exit\": Exit the program.\n");
    char* command = (char*)malloc(10); //not concerned with safe input handling, just a testbench program

    printf("This is the flipped hierarchy branch. The hash table's top level bucket is hashed by uid, and the subtable's buckets are hashed by address.\n");
    printf("How many users will this table be used by? ");
    int users;
    do
    {
        scanf("%d", &users);
        if(users < 1)
        {
            printf("Please enter a valid number of users (i.e greater than 1).\n");
        }
    } while(users < 1);

    ht* table = ht_create(users + 1); //+1 for the shared bucket
    if(table == NULL)
    {
        perror("Error: Memory allocation for main table failed\n");
        return 1;
    }

    do
    {
        printf("Enter command: ");
        scanf("%s", command);
        printf("Command entered: %s\n", command);

        if(strcmp(command, "add") == 0)
        {
            char* key = (char*)malloc(64);
            char* value = (char*)malloc(64);

            if(key == NULL || value == NULL)
            {
                perror("Error: Memory allocation failed\n");
                return 1;
            }

            int uid;
            int temp;

            printf("Enter value: ");
            scanf("%s", value);
            do
            {
                printf("Enter uid, which should be between 1 and %d : ", users);
                scanf("%d", &uid);
            } while(uid < 1 || uid > users);

            printf("uid is %d\n", uid);

            printf("Is it shared? 1 for yes, 0 for no. ");
            scanf("%d", &temp);

            bool shared = (bool)temp;

            ht_add_entry(table, key, value, uid, shared);

            ht_entry_item* subentry = get_entry_item(table, key, uid, shared);

            printf("Entry added successfully\n");
            printf("Key: %p\n", (void*)subentry->key);
            printf("Value: %s\n", (char*)subentry->value);
        }
        else if(strcmp(command, "remove") == 0)
        {
            char key_str[64];
            void* key;

            int uid;

            printf("Enter key: ");
            scanf("%s", key_str);

            printf("Enter uid: ");
            scanf("%d", &uid);

            //convert key string into pointer - otherwise the remove will look for the address of the key string, not the key corresponding to the address

            sscanf(key_str, "%p", &key);

            int remove_code = ht_remove_entry(table, key, uid);

            printf("remove code: %d\n", remove_code);

            if(remove_code == -1)
            {
                perror("Error: Entry not found\n");
                free(key);
                continue;
            }

            printf("Entry removed successfully\n");

        }
        else if(strcmp(command, "print") == 0)
        {
            int uid;
            size_t bucket_index;

            printf("Enter uid: ");
            scanf("%d", &uid);

            printf("Enter bucket index: ");
            scanf("%lu", &bucket_index);

            print_entries_in_subtable(table, uid, bucket_index);
        }   
        else if(strcmp(command, "resize") == 0)
        {
            resize_test(table);
        }
        else
        {
            printf("Invalid command\n");
        }


    } while (strcmp (command, "exit") != 0);

    return 0;
}