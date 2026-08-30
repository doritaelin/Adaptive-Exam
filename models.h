#ifndef MODELS_H
#define MODELS_H

// Module 1: User & Access Roles
typedef enum {
    STUDENT = 1,
    INSTRUCTOR = 2,
    ADMIN = 3
} UserRole;

typedef enum {
    EASY = 1,
    MEDIUM = 2,
    HARD = 3
} Difficulty;

typedef enum {
    MCQ = 1,
    SHORT_ANSWER = 2
} QuestionType;

typedef struct {
    char username[50];
    char password[50];
    UserRole role;
    char full_name[100];
} User;

// Module 3: Question Bank & CAT Items
typedef struct {
    int id;
    QuestionType type;          // MCQ or SHORT_ANSWER
    char topic[50];
    char text[300];
    char options[4][100];       // Used if MCQ
    int correct_option;         // 1-4 for MCQ
    char expected_keywords[150];// Used for NLP heuristic grading
    Difficulty difficulty;
    char explanation[300];
} Question;

// Module 4: Exam Security & Proctoring Log
typedef struct {
    char student[50];
    char violation_type[50];    // TAB_SWITCH, COPY_PASTE, WINDOW_BLUR
    char timestamp[32];
} SecurityViolation;

// Module 2 & 6: Analytics, Weakness Mapping & Reports
typedef struct {
    char student[50];
    int score;
    int total_questions;
    int easy_correct;
    int med_correct;
    int hard_correct;
    float accuracy;
    int security_flags;         // Count of tab/copy violations
    char weakness_topic[50];    // Identified knowledge gap
    char ai_feedback[256];
    char study_recommendation[256];
} ExamReport;

#endif
