#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    send(client, header, strlen(header), 0);
    send(client, body, strlen(body), 0);
}

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

// 1. REGISTER NEW STUDENT
void handle_register(SOCKET client, const char *body) {
    char user[50], pass[50];
    parse_json_str(body, "username", user, 50);
    parse_json_str(body, "password", pass, 50);

    if (strlen(user) < 3 || strlen(pass) < 4) {
        send_response(client, "400 Bad Request", "application/json", 
            "{\"success\":false,\"message\":\"Username must be >= 3 chars, password >= 4 chars\"}");
        return;
    }

    FILE *rf = fopen("users.dat", "rb");
    if (rf) {
        User existing;
        while (fread(&existing, sizeof(User), 1, rf)) {
            if (strcmp(existing.username, user) == 0) {
                fclose(rf);
                send_response(client, "409 Conflict", "application/json", 
                    "{\"success\":false,\"message\":\"Username already taken! Choose another.\"}");
                return;
            }
        }
        fclose(rf);
    }

    FILE *wf = fopen("users.dat", "ab");
    if (!wf) {
        send_response(client, "500 Internal Error", "application/json", "{\"success\":false,\"message\":\"Database write failed\"}");
        return;
    }

    User new_user;
    memset(&new_user, 0, sizeof(User));
    strncpy(new_user.username, user, 49);
    strncpy(new_user.password, pass, 49);
    new_user.role = STUDENT;

    fwrite(&new_user, sizeof(User), 1, wf);
    fclose(wf);

    send_response(client, "200 OK", "application/json", "{\"success\":true,\"message\":\"Account registered successfully!\"}");
}

// 2. LOGIN STUDENT
void handle_login(SOCKET client, const char *body) {
    char user[50], pass[50];
    parse_json_str(body, "username", user, 50);
    parse_json_str(body, "password", pass, 50);

    FILE *f = fopen("users.dat", "rb");
    if (!f) {
        send_response(client, "500 Internal Error", "application/json", "{\"error\":\"Database missing\"}");
        return;
    }

    User u;
    int found = 0;
    while (fread(&u, sizeof(User), 1, f)) {
        if (strcmp(u.username, user) == 0 && strcmp(u.password, pass) == 0) {
            found = 1;
            char res[256];
            sprintf(res, "{\"success\":true,\"username\":\"%s\",\"role\":%d}", u.username, u.role);
            send_response(client, "200 OK", "application/json", res);
            break;
        }
    }
    fclose(f);
    if (!found) send_response(client, "401 Unauthorized", "application/json", "{\"success\":false,\"message\":\"Invalid username or password!\"}");
}

// 3. FETCH STUDENT HISTORY / PROGRESS
void handle_get_history(SOCKET client, const char *body) {
    char user[50];
    parse_json_str(body, "student", user, 50);

    FILE *f = fopen("reports.dat", "rb");
    char res[8192] = "{\"success\":true,\"history\":[";
    int count = 0;

    if (f) {
        ExamReport rep;
        while (fread(&rep, sizeof(ExamReport), 1, f)) {
            if (strcmp(rep.student, user) == 0) {
                if (count > 0) strcat(res, ",");
                char item[512];
                sprintf(item, "{\"score\":%d,\"total\":%d,\"accuracy\":%.1f,\"feedback\":\"%s\"}",
                        rep.score, rep.total_questions, rep.accuracy, rep.ai_feedback);
                strcat(res, item);
                count++;
            }
        }
        fclose(f);
    }

    strcat(res, "]}");
    send_response(client, "200 OK", "application/json", res);
}

// 4. GET QUESTION (ADAPTIVE)
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
        "{\"available\":true,\"id\":%d,\"text\":\"%s\",\"options\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"difficulty\":%d,\"topic\":\"%s\"}",
        chosen.id, chosen.text, chosen.options[0], chosen.options[1], chosen.options[2], chosen.options[3], chosen.difficulty, chosen.topic);
    send_response(client, "200 OK", "application/json", res);
}

// 5. SUBMIT ANSWER
void handle_submit_answer(SOCKET client, const char *body) {
    char qid_str[10], sel_str[10], diff_str[10];
    parse_json_str(body, "id", qid_str, 10);
    parse_json_str(body, "selected", sel_str, 10);
    parse_json_str(body, "difficulty", diff_str, 10);

    int qid = atoi(qid_str);
    int selected = atoi(sel_str);
    int curr_diff = atoi(diff_str);

    FILE *f = fopen("questions.dat", "rb");
    if (!f) {
        send_response(client, "500 Internal Error", "application/json", "{\"error\":\"File error\"}");
        return;
    }

    Question q;
    int is_correct = 0;
    char explanation[300] = "";

    while (fread(&q, sizeof(Question), 1, f)) {
        if (q.id == qid) {
            if (q.correct_option == selected) is_correct = 1;
            strncpy(explanation, q.explanation, 299);
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
    sprintf(res, "{\"correct\":%s,\"points\":%d,\"nextDifficulty\":%d,\"explanation\":\"%s\"}",
            is_correct ? "true" : "false", score_delta, next_diff, explanation);
    send_response(client, "200 OK", "application/json", res);
}

// 6. SAVE REPORT
void handle_save_report(SOCKET client, const char *body) {
    char user[50], score_s[10], total_s[10], ec[10], mc[10], hc[10], acc_s[10];
    parse_json_str(body, "student", user, 50);
    parse_json_str(body, "score", score_s, 10);
    parse_json_str(body, "total", total_s, 10);
    parse_json_str(body, "easy_correct", ec, 10);
    parse_json_str(body, "med_correct", mc, 10);
    parse_json_str(body, "hard_correct", hc, 10);
    parse_json_str(body, "accuracy", acc_s, 10);

    ExamReport rep;
    strncpy(rep.student, user, 49);
    rep.score = atoi(score_s);
    rep.total_questions = atoi(total_s);
    rep.easy_correct = atoi(ec);
    rep.med_correct = atoi(mc);
    rep.hard_correct = atoi(hc);
    rep.accuracy = (float)atof(acc_s);

    if (rep.accuracy >= 80.0f) {
        strcpy(rep.ai_feedback, "Exceptional Mastery. High proficiency under advanced cognitive loads.");
    } else if (rep.accuracy >= 50.0f) {
        strcpy(rep.ai_feedback, "Solid Competency. Consistent performance with potential in complex logic.");
    } else {
        strcpy(rep.ai_feedback, "Foundational. Reinforce memory fundamentals before stepping up.");
    }

    FILE *f = fopen("reports.dat", "ab");
    if (f) {
        fwrite(&rep, sizeof(ExamReport), 1, f);
        fclose(f);
    }

    char res[512];
    sprintf(res, "{\"status\":\"saved\",\"ai_feedback\":\"%s\"}", rep.ai_feedback);
    send_response(client, "200 OK", "application/json", res);
}

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
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    send_response(client, "200 OK", content_type, buf);
    free(buf);
}

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
    printf("   AI ADAPTIVE EXAMINATION PLATFORM (C ENGINE)         \n");
    printf("   Local Server Live: http://localhost:%d              \n", PORT);
    printf("=======================================================\n\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        SOCKET client = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client == INVALID_SOCKET) continue;

        char buffer[BUFFER_SIZE] = {0};
        recv(client, buffer, BUFFER_SIZE - 1, 0);

        if (strstr(buffer, "OPTIONS")) {
            send_response(client, "200 OK", "text/plain", "");
        } else if (strncmp(buffer, "GET / ", 6) == 0 || strncmp(buffer, "GET /index.html", 15) == 0) {
            serve_static(client, "public/index.html", "text/html");
        } else if (strstr(buffer, "POST /api/register")) {
            char *body = strstr(buffer, "\r\n\r\n");
            handle_register(client, body ? body + 4 : "");
        } else if (strstr(buffer, "POST /api/login")) {
            char *body = strstr(buffer, "\r\n\r\n");
            handle_login(client, body ? body + 4 : "");
        } else if (strstr(buffer, "POST /api/history")) {
            char *body = strstr(buffer, "\r\n\r\n");
            handle_get_history(client, body ? body + 4 : "");
        } else if (strstr(buffer, "POST /api/question")) {
            char *body = strstr(buffer, "\r\n\r\n");
            handle_adaptive_question(client, body ? body + 4 : "");
        } else if (strstr(buffer, "POST /api/submit")) {
            char *body = strstr(buffer, "\r\n\r\n");
            handle_submit_answer(client, body ? body + 4 : "");
        } else if (strstr(buffer, "POST /api/report")) {
            char *body = strstr(buffer, "\r\n\r\n");
            handle_save_report(client, body ? body + 4 : "");
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