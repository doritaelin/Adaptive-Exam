#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"

void seed_users() {
    FILE *f = fopen("users.dat", "wb");
    if (!f) {
        printf("Error opening users.dat\n");
        return;
    }
    User users[] = {
        {"student1", "pass123", STUDENT, "John Doe"},
        {"instructor1", "admin123", INSTRUCTOR, "Prof. Alan Turing"},
        {"admin", "supersecret", ADMIN, "Chief Administrator"}
    };
    int n = sizeof(users) / sizeof(users[0]);
    fwrite(users, sizeof(User), n, f);
    fclose(f);
    printf("Successfully seeded %d users.\n", n);
}

void seed_questions() {
    FILE *f = fopen("questions.dat", "wb");
    if (!f) {
        printf("Error opening questions.dat\n");
        return;
    }

    Question bank[] = {
        // EASY QUESTIONS (MCQ)
        {1, MCQ, "Variables", "Which data type is used to store whole numbers in C?", {"float", "char", "int", "double"}, 3, "", EASY, "The 'int' keyword stores integer values in C."},
        {2, MCQ, "Syntax", "What symbol ends every statement in C?", {":", ";", ".", ","}, 2, "", EASY, "Statements in C must terminate with a semicolon."},
        {3, MCQ, "Control Flow", "Which keyword is used for conditional decision making?", {"if", "for", "while", "return"}, 1, "", EASY, "The 'if' keyword executes code conditionally."},

        // MEDIUM QUESTIONS (MCQ)
        {4, MCQ, "Pointers", "What operator is used to get the memory address of a variable?", {"*", "&", "%", "->"}, 2, "", MEDIUM, "The ampersand '&' is the address-of operator."},
        {5, MCQ, "Memory Management", "Which standard library function dynamically allocates memory?", {"free()", "malloc()", "sizeof()", "printf()"}, 2, "", MEDIUM, "malloc() dynamically allocates a specified byte block on the heap."},
        {6, MCQ, "Strings", "What is the terminating character of a C string?", {"\\n", "\\t", "\\0", "\\r"}, 3, "", MEDIUM, "C strings are null-terminated with '\\0'."},

        // HARD QUESTIONS (MCQ & Short Answer)
        {7, MCQ, "Pointers", "What is a pointer that points to deallocated memory called?", {"Null pointer", "Void pointer", "Dangling pointer", "Wild pointer"}, 3, "", HARD, "A dangling pointer points to memory already deallocated."},
        {8, MCQ, "Advanced C", "What does the 'volatile' keyword tell the compiler?", {"Store in cache", "Do not optimize away reads", "Make read-only", "Make static"}, 2, "", HARD, "'volatile' prevents the compiler from optimizing variable access."},
        {9, MCQ, "Memory Safety", "What happens if you access outside array bounds in C?", {"Crash warning", "Undefined behavior", "Array auto-grows", "Syntax error"}, 2, "", HARD, "Array bounds are unchecked in standard C, causing undefined behavior."},
        {10, SHORT_ANSWER, "Dynamic Memory", "Explain why you must free dynamically allocated memory in C.", {"", "", "", ""}, 0, "leak,memory leak,free,heap,deallocate", HARD, "Failing to free heap memory causes memory leaks."}
    };

    int n = sizeof(bank) / sizeof(bank[0]);
    fwrite(bank, sizeof(Question), n, f);
    fclose(f);
    printf("Successfully seeded %d questions into bank.\n", n);
}

int main() {
    seed_users();
    seed_questions();
    return 0;
}
