#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "models.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #define closesocket close
  #define SOCKET int
  #define INVALID_SOCKET -1
#endif

#define PORT 8080
#define BUFFER_SIZE 16384

// Utility: HTTP Response Sender
void send_response(SOCKET client, const char *status, const char *content_type, const char *body) {
    char header[1024];
    sprintf(header, 
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status, content_type, strlen(body));
    send(client, header, (int)strlen(header), 0);
    send(client, body, (int)strlen(body), 0);
}

// Utility: JSON String Parser
void parse_json_str(const char *json, const char *key, char *out, int max_len) {
    char pattern[128];
    sprintf(pattern, "\"%s\":", key);
    char *pos = strstr(json, pattern);
    if (!pos) { out[0] = '\0'; return; }
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\"') pos++;
    int i = 0;
    while (*pos && *pos != '\"' && *pos != ',' && *pos != '}' && i < max_len - 1) {
        out[i++] = *pos++;
    }
    out[i] = '\0';
}

// Case-insensitive substring helper for NLP heuristic evaluation
int contains_ignore_case(const char *haystack, const char *needle) {
    char h[512], n[128];
    int i = 0;
    for (; haystack[i] && i < 511; i++) h[i] = (char)tolower((unsigned char)haystack[i]);
    h[i] = '\0';
    i = 0;
    for (; needle[i] && i < 127; i++) n[i] = (char)tolower((unsigned char)needle[i]);
    n[i] = '\0';
    return (strstr(h, n) != NULL);
}

// ============================================================================
// MODULE 1: User & Access Management (Student, Instructor, Admin)
// ============================================================================
void handle_register(SOCKET client, const char *body) {
    char user[50], pass[50], role_str[10], name[100];
    parse_json_str(body, "username", user, 50);
    parse_json_str(body, "password", pass, 50);
    parse_json_str(body, "role", role_str, 10);
    parse_json_str(body, "name", name, 100);

    int role_val = atoi(role_str);
    if (role_val < STUDENT || role_val > ADMIN) role_val = STUDENT;

    if (strlen(user) < 3 || strlen(pass) < 4) {
        send_response(client, "400 Bad Request", "application/json", 
            "{\"success\":false,\"message\":\"Username >= 3 chars, password >= 4 chars required\"}");
        return;
    }

    FILE *rf = fopen("users.dat", "rb");
    if (rf) {
        User existing;
        while (fread(&existing, sizeof(User), 1, rf)) {
            if (strcmp(existing.username, user) == 0) {
                fclose(rf);
                send_response(client, "409 Conflict", "application/json", 
                    "{\"success\":false,\"message\":\"Username already taken\"}");
                return;
            }
        }
        fclose(rf);
    }

    FILE *wf = fopen("users.dat", "ab");
    if (!wf) {
        send_response(client, "500 Internal Error", "application/json", "{\"success\":false,\"message\":\"Database write error\"}");
        return;
    }

    User new_user;
    memset(&new_user, 0, sizeof(User));
    strncpy(new_user.username, user, 49);
    strncpy(new_user.password, pass, 49);
    strncpy(new_user.full_name, strlen(name) > 0 ? name : user, 99);
    new_user.role = (UserRole)role_val;

    fwrite(&new_user, sizeof(User), 1, wf);
    fclose(wf);

    send_response(client, "200 OK", "application/json", "{\"success\":true,\"message\":\"Account registered successfully!\"}");
}

void handle_login(SOCKET client, const char *body) {
    char user[50], pass[50];
    parse_json_str(body, "username", user, 50);
    parse_json_str(body, "password", pass, 50);

    FILE *f = fopen("users.dat", "rb");
    if (!f) {
        send_response(client, "500 Internal Error", "application/json", "{\"error\":\"User database not found\"}");
        return;
    }

    User u;
    int found = 0;
    while (fread(&u, sizeof(User), 1, f)) {
        if (strcmp(u.username, user) == 0 && strcmp(u.password, pass) == 0) {
            found = 1;
            char res[512];
            sprintf(res, "{\"success\":true,\"username\":\"%s\",\"role\":%d,\"name\":\"%s\"}", u.username, u.role, u.full_name);
            send_response(client, "200 OK", "application/json", res);
            break;
        }
    }
    fclose(f);
    if (!found) send_response(client, "401 Unauthorized", "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
}

// ============================================================================
// MODULE 3: Question Bank & Dynamic Test Generator (CAT & Randomization)
// ============================================================================
void handle_adaptive_question(SOCKET client, const char *body) {
    char diff_str[10], excluded_str[512];
    parse_json_str(body, "difficulty", diff_str, 10);
    parse_json_str(body, "excluded", excluded_str, 512);

    int target_diff = atoi(diff_str);
    if (target_diff < EASY) target_diff = EASY;
    if (target_diff > HARD) target_diff = HARD;

    FILE *f = fopen("questions.dat", "rb");
    if (!f) {
        send_response(client, "500 Internal Error", "application/json", "{\"error\":\"Question Bank missing\"}");
        return;
    }

    Question q;
    Question pool[50];
    int count = 0;

    char padded_excl[600];
    sprintf(padded_excl, ",%s,", excluded_str);

    while (fread(&q, sizeof(Question), 1, f)) {
        char id_str[32];
        sprintf(id_str, ",%d,", q.id);
        if (q.difficulty == target_diff && strstr(padded_excl, id_str) == NULL) {
            pool[count++] = q;
            if (count >= 50) break;
        }
    }
    fclose(f);

    if (count == 0) {
        send_response(client, "200 OK", "application/json", "{\"available\":false}");
        return;
    }

    Question chosen = pool[rand() % count];
    char res[2048];
    sprintf(res, 
        "{\"available\":true,\"id\":%d,\"type\":%d,\"text\":\"%s\",\"options\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"difficulty\":%d,\"topic\":\"%s\"}",
        chosen.id, chosen.type, chosen.text, chosen.options[0], chosen.options[1], chosen.options[2], chosen.options[3], chosen.difficulty, chosen.topic);
    send_response(client, "200 OK", "application/json", res);
}

// ============================================================================
// MODULE 4: AI Exam Security & Proctoring Logger
// ============================================================================
void handle_security_violation(SOCKET client, const char *body) {
    char user[50], violation[50];
    parse_json_str(body, "student", user, 50);
    parse_json_str(body, "violation", violation, 50);

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    if (time_str) time_str[strcspn(time_str, "\r\n")] = 0;

    SecurityViolation sv;
    memset(&sv, 0, sizeof(SecurityViolation));
    strncpy(sv.student, user, 49);
    strncpy(sv.violation_type, violation, 49);
    strncpy(sv.timestamp, time_str ? time_str : "N/A", 31);

    FILE *f = fopen("security_audit.dat", "ab");
    if (f) {
        fwrite(&sv, sizeof(SecurityViolation), 1, f);
        fclose(f);
    }

    send_response(client, "200 OK", "application/json", "{\"logged\":true,\"status\":\"Security incident flagged\"}");
}

// ============================================================================
// MODULE 5: Automated Grading & Evaluation Engine (Objective & NLP Grading)
// ============================================================================
void handle_submit_answer(SOCKET client, const char *body) {
    char qid_str[10], sel_str[10], text_answer[256], diff_str[10];
    parse_json_str(body, "id", qid_str, 10);
    parse_json_str(body, "selected", sel_str, 10);
    parse_json_str(body, "text_answer", text_answer, 256);
    parse_json_str(body, "difficulty", diff_str, 10);

    int qid = atoi(qid_str);
    int selected = atoi(sel_str);
    int curr_diff = atoi(diff_str);

    FILE *f = fopen("questions.dat", "rb");
    if (!f) {
        send_response(client, "500 Internal Error", "application/json", "{\"error\":\"Question Bank missing\"}");
        return;
    }

    Question q;
    int is_correct = 0;
    char explanation[300] = "";
    char topic[50] = "";

    while (fread(&q, sizeof(Question), 1, f)) {
        if (q.id == qid) {
            strncpy(explanation, q.explanation, 299);
            strncpy(topic, q.topic, 49);

            if (q.type == MCQ) {
                if (q.correct_option == selected) is_correct = 1;
            } else {
                char temp_keywords[150];
                strncpy(temp_keywords, q.expected_keywords, 149);
                char *token = strtok(temp_keywords, ",");
                int matches = 0;
                while (token != NULL) {
                    while (*token == ' ') token++;
                    if (strlen(token) > 0 && contains_ignore_case(text_answer, token)) {
                        matches++;
                    }
                    token = strtok(NULL, ",");
                }
                if (matches >= 1) is_correct = 1;
            }
            break;
        }
    }
    fclose(f);

    int next_diff = curr_diff;
    int score_delta = 0;

    if (is_correct) {
        score_delta = curr_diff * 10;
        if (next_diff < HARD) next_diff++;
    } else {
        score_delta = 0;
        if (next_diff > EASY) next_diff--;
    }

    char res[512];
    sprintf(res, "{\"correct\":%s,\"points\":%d,\"nextDifficulty\":%d,\"topic\":\"%s\",\"explanation\":\"%s\"}",
            is_correct ? "true" : "false", score_delta, next_diff, topic, explanation);
    send_response(client, "200 OK", "application/json", res);
}

// ============================================================================
// MODULE 2 & 6: Adaptive Learning Engine & Comprehensive Analytics Module
// ============================================================================
void handle_save_report(SOCKET client, const char *body) {
    char user[50], score_s[10], total_s[10], ec[10], mc[10], hc[10], acc_s[10], sec_flags[10], weak_topic[50];
    parse_json_str(body, "student", user, 50);
    parse_json_str(body, "score", score_s, 10);
    parse_json_str(body, "total", total_s, 10);
    parse_json_str(body, "easy_correct", ec, 10);
    parse_json_str(body, "med_correct", mc, 10);
    parse_json_str(body, "hard_correct", hc, 10);
    parse_json_str(body, "accuracy", acc_s, 10);
    parse_json_str(body, "security_flags", sec_flags, 10);
    parse_json_str(body, "weakness_topic", weak_topic, 50);

    ExamReport rep;
    memset(&rep, 0, sizeof(ExamReport));
    strncpy(rep.student, user, 49);
    rep.score = atoi(score_s);
    rep.total_questions = atoi(total_s);
    rep.easy_correct = atoi(ec);
    rep.med_correct = atoi(mc);
    rep.hard_correct = atoi(hc);
    rep.accuracy = (float)atof(acc_s);
    rep.security_flags = atoi(sec_flags);
    strncpy(rep.weakness_topic, strlen(weak_topic) > 0 ? weak_topic : "Core Concepts", 49);

    if (rep.accuracy >= 80.0f) {
        strcpy(rep.ai_feedback, "Exceptional Mastery. High proficiency under advanced cognitive loads.");
        sprintf(rep.study_recommendation, "Ready for Advanced Multi-threading & Low-level System Design.");
    } else if (rep.accuracy >= 50.0f) {
        strcpy(rep.ai_feedback, "Solid Competency. Consistent performance with potential in complex logic.");
        sprintf(rep.study_recommendation, "Review knowledge gap in: %s. Practice pointer memory mapping.", rep.weakness_topic);
    } else {
        strcpy(rep.ai_feedback, "Foundational. Reinforce memory fundamentals before stepping up.");
        sprintf(rep.study_recommendation, "Targeted Review Required: %s basics and conditional structures.", rep.weakness_topic);
    }

    FILE *f = fopen("reports.dat", "ab");
    if (f) {
        fwrite(&rep, sizeof(ExamReport), 1, f);
        fclose(f);
    }

    char res[1024];
    sprintf(res, "{\"status\":\"saved\",\"ai_feedback\":\"%s\",\"recommendation\":\"%s\",\"weakness\":\"%s\"}",
            rep.ai_feedback, rep.study_recommendation, rep.weakness_topic);
    send_response(client, "200 OK", "application/json", res);
}

void handle_get_history(SOCKET client, const char *body) {
    char user[50];
    parse_json_str(body, "student", user, 50);

    FILE *f = fopen("reports.dat", "rb");
    char res[16384] = "{\"success\":true,\"history\":[";
    int count = 0;

    if (f) {
        ExamReport rep;
        while (fread(&rep, sizeof(ExamReport), 1, f)) {
            if (strcmp(rep.student, user) == 0 || strlen(user) == 0) {
                if (count > 0) strcat(res, ",");
                char item[512];
                sprintf(item, "{\"student\":\"%s\",\"score\":%d,\"total\":%d,\"accuracy\":%.1f,\"security_flags\":%d,\"weakness\":\"%s\",\"feedback\":\"%s\",\"recommendation\":\"%s\"}",
                        rep.student, rep.score, rep.total_questions, rep.accuracy, rep.security_flags, rep.weakness_topic, rep.ai_feedback, rep.study_recommendation);
                strcat(res, item);
                count++;
            }
        }
        fclose(f);
    }

    strcat(res, "]}");
    send_response(client, "200 OK", "application/json", res);
}

// Static File Server
void serve_static(SOCKET client, const char *filepath, const char *content_type) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        send_response(client, "404 Not Found", "text/plain", "Static asset not found");
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        send_response(client, "500 Internal Error", "text/plain", "Memory allocation failed");
        return;
    }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    send_response(client, "200 OK", content_type, buf);
    free(buf);
}

// Server Main Routing Loop
int main() {
    srand((unsigned int)time(NULL));
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 20);

    printf("\n=======================================================\n");
    printf("   FULL 6-MODULE ADAPTIVE EXAM SERVER (C ENGINE)       \n");
    printf("   Modules: Auth | CAT | Security | NLP | Analytics    \n");
    printf("   Server Live: http://localhost:%d                   \n", PORT);
    printf("=======================================================\n\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        SOCKET client = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client == INVALID_SOCKET) continue;

        char buffer[BUFFER_SIZE] = {0};
        recv(client, buffer, BUFFER_SIZE - 1, 0);

        char *body = strstr(buffer, "\r\n\r\n");
        const char *payload = body ? body + 4 : "";

        if (strstr(buffer, "OPTIONS")) {
            send_response(client, "200 OK", "text/plain", "");
        } else if (strncmp(buffer, "GET / ", 6) == 0 || strncmp(buffer, "GET /index.html", 15) == 0) {
            serve_static(client, "public/index.html", "text/html");
        } else if (strstr(buffer, "POST /api/register")) {
            handle_register(client, payload);
        } else if (strstr(buffer, "POST /api/login")) {
            handle_login(client, payload);
        } else if (strstr(buffer, "POST /api/question")) {
            handle_adaptive_question(client, payload);
        } else if (strstr(buffer, "POST /api/submit")) {
            handle_submit_answer(client, payload);
        } else if (strstr(buffer, "POST /api/security/violation")) {
            handle_security_violation(client, payload);
        } else if (strstr(buffer, "POST /api/report")) {
            handle_save_report(client, payload);
        } else if (strstr(buffer, "POST /api/history")) {
            handle_get_history(client, payload);
        } else {
            send_response(client, "404 Not Found", "text/plain", "Route not found");
        }

        closesocket(client);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}