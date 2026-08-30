#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"

int main() {
    // 1. Create user account (student1 / pass123)
    FILE *uf = fopen("users.dat", "wb");
    if (!uf) { printf("Error creating users.dat\n"); return 1; }

    User users[] = {
        {"student1", "pass123", STUDENT},
        {"admin", "admin123", ADMIN}
    };
    fwrite(users, sizeof(User), 2, uf);
    fclose(uf);

    // 2. Create the Question Bank
    FILE *qf = fopen("questions.dat", "wb");
    if (!qf) { printf("Error creating questions.dat\n"); return 1; }

    Question bank[] = {
        // EASY QUESTIONS
        {1, "What is standard for printing text in C?", {"printf()", "cout", "System.out.print", "echo"}, 1, EASY, "C Basics", "printf() is used in C to print output."},
        {2, "Which header file is needed for printf()?", {"<stdio.h>", "<stdlib.h>", "<math.h>", "<string.h>"}, 1, EASY, "C Basics", "stdio stands for Standard Input Output."},
        {3, "What symbol ends every statement in C?", {":", ";", ".", "!"}, 2, EASY, "C Basics", "Statements in C must end with a semicolon ;"},

        // MEDIUM QUESTIONS
        {4, "Which operator gets the memory address of a variable?", {"*", "&", "%", "->"}, 2, MEDIUM, "Pointers", "The ampersand & returns the address."},
        {5, "What function allocates dynamic memory in C?", {"malloc()", "create()", "alloc()", "new"}, 1, MEDIUM, "Memory", "malloc() allocates heap memory."},
        {6, "What is the index of the very first element in a C array?", {"1", "0", "-1", "Depends"}, 2, MEDIUM, "Arrays", "Arrays in C start at index 0."},

        // HARD QUESTIONS
        {7, "What is a pointer that points to freed memory called?", {"Null pointer", "Void pointer", "Dangling pointer", "Wild pointer"}, 3, HARD, "Advanced C", "A dangling pointer points to memory already deallocated."},
        {8, "What does the 'volatile' keyword tell the compiler?", {"Store in cache", "Do not optimize away reads", "Make read-only", "Make static"}, 2, HARD, "Advanced C", "volatile tells compiler variable can change unexpectedly."},
        {9, "What happens if you exceed array bounds in C?", {"Crash warning", "Undefined behavior", "Array auto-grows", "Syntax error"}, 2, HARD, "Memory Safety", "C does not do bounds checking, leading to undefined behavior."}
    };

    fwrite(bank, sizeof(Question), sizeof(bank)/sizeof(Question), qf);
    fclose(qf);

    printf("SUCCESS! Secret database files (users.dat & questions.dat) are ready!\n");
    return 0;
}