#ifndef MODELS_H
#define MODELS_H

typedef enum { EASY = 1, MEDIUM = 2, HARD = 3 } Difficulty;
typedef enum { STUDENT = 1, ADMIN = 2 } Role;

// Holds login details
typedef struct {
    char username[50];
    char password[50];
    int role;
} User;

// Holds one quiz question
typedef struct {
    int id;
    char text[300];
    char options[4][150];
    int correct_option; // 1, 2, 3, or 4
    int difficulty;     // 1: Easy, 2: Medium, 3: Hard
    char topic[50];
    char explanation[300];
} Question;

// Holds your final report card
typedef struct {
    char student[50];
    int score;
    int total_questions;
    int easy_correct;
    int med_correct;
    int hard_correct;
    float accuracy;
    char ai_feedback[256];
} ExamReport;

#endif