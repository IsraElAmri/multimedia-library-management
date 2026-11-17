#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
static int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

#define MAX_BORROWED 5
#define MAX_LATE_RETURNS_ALLOWED 5
#define NORMAL_RESET_MINUTES 60      // 1 hour reset for regular mode
#define STRICT_RESET_MINUTES 1440    // 24 hours for high-security
#define MAX_LOGIN_ATTEMPTS 3         // Attempts before temporary lockout
#define MAX_FAILED_ATTEMPTS 9        // Attempts before permanent lockout
#define BORROW_DAYS_LIMIT 14
#define SECURITY_FILE "security.dat"
#define CREDENTIALS_FILE "admin_credentials.dat"

#define MAX_INPUT_LENGTH 100
#define MAX_MEDIA 1000
#define ID_CARD_LENGTH 14  // Standard ID card length for many countries

// Enums for media types
typedef enum { BOOK, MAGAZINE, NEWSPAPER } ReadType;
typedef enum { MOVIE, TV_SHOW, DOCUMENTARY } WatchType;
typedef enum { MUSIC, PODCAST, AUDIOBOOK } HearType;

// Media structs
typedef struct {
    char title[50];
    char author[30];
    char category[20];
    int pages;
    char publisher[30];
    int year;
    ReadType type;
    int isBorrowed;
    time_t borrowDate;
    char borrower[30];
} ReadingMaterial;

typedef struct {
    char title[50];
    char director[30];
    char category[30];
    int durationMinutes;
    int durationSeconds;
    char releaseDate[20]; // "YYYY-MM-DD"
    WatchType type;
    int isBorrowed;
    time_t borrowDate;
    char borrower[30];
} WatchingMaterial;

typedef struct {
    char title[50];
    char artist[30];
    char category[30];
    int durationMinutes;
    int durationSeconds;  
    int year;
    HearType type;
    int isBorrowed;
    time_t borrowDate;
    char borrower[30];
} ListeningMaterial;

// Linked list node for media
typedef struct MediaNode {
    char mediaType[10];  // "Read", "Watch", "Hear"
    int id;
    union {
        ReadingMaterial read;
        WatchingMaterial watch;
        ListeningMaterial hear;
    };
    struct MediaNode *next;
} Media;

// Client structure
typedef struct {
    char idCard[20];
    char firstName[30];
    char lastName[30];
    int lateReturnCount;
    struct {
        int mediaId;
        time_t borrowDate;
        time_t returnDate; // 0 if not returned
    } borrowedItems[MAX_BORROWED];
} Client;

typedef struct {
    time_t lockoutEndTime;
    int totalFailedLogins;
    time_t firstFailedLoginTime;
    int currentLockoutDuration;
} SecuritySettings;

typedef struct {
    char username[50];
    char password[50];
    char passwordHint[100];
} AdminCredentials;

/* ========== GLOBALS ========== */
Media *mediaLibrary = NULL;
int lastMediaId = 0;
int isAdminLoggedIn = 0;

// Security variables
int currentLoginAttempts = 0;
int totalFailedLogins = 0;
time_t temporaryLockoutTime = 0;
time_t firstFailedLoginTime = 0;
time_t permanentLockoutEndTime = 0;
int currentLockoutDuration = NORMAL_RESET_MINUTES;

/* ========== Function prototypes ========== */
int searchAndSelectMedia(int allowBack);
int getMediaIdWithOptions(const char *action);

/* ========== UTILITY FUNCTIONS ========== */
void clearInputBuffer() {
    while (getchar() != '\n');
}

void pressAnyKey() {
    printf("\nPress any key to continue...\n");
    getch();
}

void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif
}

void printMainTitle() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timeStr[80];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);

    printf("=== Media Library Management System ===\n");
    if (isAdminLoggedIn) {
        printf("Admin mode | ");
    }
    printf("Current time: %s\n", timeStr);
    printf("======================================\n");
}

int caseInsensitiveCompare(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff) return diff;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

char *caseInsensitiveStrStr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;

    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

void formatTunisianId(char *id) {
    if (strlen(id) == 8) {
        char temp[12];
        sprintf(temp, "TN-%s", id);
        strcpy(id, temp);
    }
}

int isValidIdCard(const char *idCard) {
    if (!idCard) return 0;

    int len = strlen(idCard);
    
    /* Case 1: Tunisian ID (8 digits) */
    if (len == 8) {
        for (int i = 0; i < 8; i++) {
            if (!isdigit(idCard[i])) {
                return 0;
            }
        }
        return 1;
    }

    /* Case 2: Library format (AA-123456789-01) */
    if (len == 15 && 
        isalpha(idCard[0]) && 
        isalpha(idCard[1]) &&
        idCard[2] == '-' &&
        isdigit(idCard[3]) && 
        isdigit(idCard[12]) &&
        idCard[13] == '-' &&
        isdigit(idCard[14])) {
        return 1;
    }

    return 0;
}

int getValidId(const char *prompt, char *buf, size_t max_len) {
    while (1) {
        printf("%s (8-digit Tunisian ID or AA-123456789-01 format, 0 to cancel): ", prompt);

        if (!fgets(buf, max_len, stdin)) {
            clearInputBuffer();
            printf("Input error, try again.\n");
            continue;
        }

        char *newline = strchr(buf, '\n');
        if (newline) *newline = '\0';

        if (strcmp(buf, "0") == 0) {
            buf[0] = '\0';
            return 0;
        }

        if (isValidIdCard(buf)) {
            return 1;
        }

        printf("Invalid ID. Must be:\n"
               "- 8-digit Tunisian ID (e.g., 12345678)\n"
               "- Or library format (e.g., AB-123456789-01)\n");
    }
}

int getValidInt(const char *prompt, int min, int max) {
    char buffer[MAX_INPUT_LENGTH];
    int value;

    while (1) {
        printf("%s (%d-%d, or 0 to go back): ", prompt, min, max);
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            clearInputBuffer();
            printf("Input error, try again.\n");
            continue;
        }

        if (sscanf(buffer, "%d", &value) != 1) {
            printf("Invalid input. Enter a number.\n");
            continue;
        }

        if (value == 0) return 0;
        if (value >= min && value <= max) return value;
        printf("Input must be between %d and %d\n", min, max);
    }
}

int getValidString(const char *prompt, char *buf, size_t max_len) {
    while (1) {
        printf("%s (or 0 to go back): ", prompt);
        if (!fgets(buf, max_len, stdin)) {
            clearInputBuffer();
            printf("Input error, try again.\n");
            continue;
        }

        buf[strcspn(buf, "\n")] = '\0';

        if (strcmp(buf, "0") == 0) {
            buf[0] = '\0';
            return 0;
        }

        if (strlen(buf) > 0) return 1;
        printf("Input cannot be empty.\n");
    }
}

int isMediaBorrowed(Media *media) {
    if (!media) return 0;

    if (strcmp(media->mediaType, "Read") == 0) {
        return media->read.isBorrowed;
    } else if (strcmp(media->mediaType, "Watch") == 0) {
        return media->watch.isBorrowed;
    } else if (strcmp(media->mediaType, "Hear") == 0) {
        return media->hear.isBorrowed;
    }
    return 0;
}

void setMediaBorrowedStatus(Media *media, int status, const char *borrower) {
    if (!media) return;

    if (strcmp(media->mediaType, "Read") == 0) {
        media->read.isBorrowed = status;
        if (borrower) strncpy(media->read.borrower, borrower, sizeof(media->read.borrower) - 1);
        if (status) media->read.borrowDate = time(NULL);
    } else if (strcmp(media->mediaType, "Watch") == 0) {
        media->watch.isBorrowed = status;
        if (borrower) strncpy(media->watch.borrower, borrower, sizeof(media->watch.borrower) - 1);
        if (status) media->watch.borrowDate = time(NULL);
    } else if (strcmp(media->mediaType, "Hear") == 0) {
        media->hear.isBorrowed = status;
        if (borrower) strncpy(media->hear.borrower, borrower, sizeof(media->hear.borrower) - 1);
        if (status) media->hear.borrowDate = time(NULL);
    }
}

int getMediaIdWithOptions(const char *action) {
    printf("\nHow do you want to %s media?\n", action);
    printf("1. Enter ID directly\n");
    printf("2. Search for media\n");
    printf("0. Cancel\n");

    int choice = getValidInt("Select option", 0, 2);
    if (choice == 0) return 0;

    if (choice == 1) {
        int maxId = 0;
        Media *current = mediaLibrary;
        while (current != NULL) {
            if (current->id > maxId) {
                maxId = current->id;
            }
            current = current->next;
        }
        
        if (maxId == 0) {
            printf("No media available to %s.\n", action);
            pressAnyKey();
            return 0;
        }

        while (1) {
            printf("Enter media ID (0 to cancel): ");
            char input[MAX_INPUT_LENGTH];
            if (!fgets(input, sizeof(input), stdin)) {
                clearInputBuffer();
                printf("Input error. Please try again.\n");
                continue;
            }

            int id;
            if (sscanf(input, "%d", &id) != 1) {
                printf("Invalid input. Please enter a number.\n");
                continue;
            }

            if (id == 0) return 0;

            current = mediaLibrary;
            while (current != NULL) {
                if (current->id == id) {
                    return id;
                }
                current = current->next;
            }

            printf("Media with ID %d not found. Please try again.\n", id);
        }
    } else {
        return searchAndSelectMedia(1);
    }
}

void printDuration(int totalMinutes, int seconds) {
    seconds = (seconds < 0 || seconds > 59) ? 0 : seconds;

    if (totalMinutes >= 60) {
        printf("%dh %02dm", totalMinutes/60, totalMinutes%60);
    } else {
        printf("%dm", totalMinutes);
    }

    if (seconds > 0) {
        printf(" %02ds", seconds);
    }
}

int getDurationComponent(const char* prompt, int min, int max) {
    char buffer[MAX_INPUT_LENGTH];
    int value;

    while (1) {
        printf("%s (%d-%d): ", prompt, min, max);
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            clearInputBuffer();
            printf("Input error, try again.\n");
            continue;
        }

        if (sscanf(buffer, "%d", &value) != 1) {
            printf("Invalid input. Enter a number.\n");
            continue;
        }

        if (value >= min && value <= max) {
            return value;
        }
        printf("Input must be between %d and %d\n", min, max);
    }
}

void getDurationInput(int* minutes, int* seconds) {
    printf("\nEnter duration:\n");
    int hours = getDurationComponent("  Hours (0-99)", 0, 99);
    int mins = getDurationComponent("  Minutes (0-59)", 0, 59);
    int secs = getDurationComponent("  Seconds (0-59)", 0, 59);

    *minutes = hours * 60 + mins;
    *seconds = secs;
}

int getValidDate(const char* prompt, char* buf, size_t max_len) {
    while (1) {
        printf("%s (YYYY-MM-DD): ", prompt);
        if (!fgets(buf, max_len, stdin)) {
            clearInputBuffer();
            printf("Input error, try again.\n");
            continue;
        }

        buf[strcspn(buf, "\n")] = '\0';
        
        if (strlen(buf) != 10 || buf[4] != '-' || buf[7] != '-') {
            printf("Invalid format. Please use YYYY-MM-DD format.\n");
            continue;
        }
        
        int year, month, day;
        if (sscanf(buf, "%d-%d-%d", &year, &month, &day) != 3) {
            printf("Invalid date components.\n");
            continue;
        }
        
        if (year < 1900 || year > 2100) {
            printf("Year must be between 1900-2100.\n");
            continue;
        }
        if (month < 1 || month > 12) {
            printf("Month must be between 01-12.\n");
            continue;
        }
        if (day < 1 || day > 31) {
            printf("Day must be between 01-31.\n");
            continue;
        }

        if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
            printf("This month only has 30 days.\n");
            continue;
        }

        if (month == 2) {
            int isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            if (day > (isLeap ? 29 : 28)) {
                printf("February only has %d days in %d.\n", isLeap ? 29 : 28, year);
                continue;
            }
        }

        return 1;
    }
}

void saveSecuritySettings() {
    FILE *fp = fopen(SECURITY_FILE, "wb");
    if (fp) {
        SecuritySettings settings;
        settings.lockoutEndTime = permanentLockoutEndTime;
        settings.totalFailedLogins = totalFailedLogins;
        settings.firstFailedLoginTime = firstFailedLoginTime;
        settings.currentLockoutDuration = currentLockoutDuration;

        fwrite(&settings, sizeof(SecuritySettings), 1, fp);
        fclose(fp);
    }
}

void loadSecuritySettings() {
    FILE *fp = fopen(SECURITY_FILE, "rb");
    if (fp) {
        SecuritySettings settings;
        if (fread(&settings, sizeof(SecuritySettings), 1, fp) == 1) {
            permanentLockoutEndTime = settings.lockoutEndTime;
            totalFailedLogins = settings.totalFailedLogins;
            firstFailedLoginTime = settings.firstFailedLoginTime;
            currentLockoutDuration = settings.currentLockoutDuration;

            // Check if we should reset the failed attempts counter
            time_t now = time(NULL);
            if (totalFailedLogins > 0 && 
                (now - firstFailedLoginTime) >= (currentLockoutDuration * 60)) {
                totalFailedLogins = 0;
                firstFailedLoginTime = 0;
                currentLoginAttempts = 0;
                saveSecuritySettings();
            }
        }
        fclose(fp);
    } else {
        // Initialize with defaults if file doesn't exist
        currentLockoutDuration = NORMAL_RESET_MINUTES;
    }
}

void configureSecuritySettings() {
    if (!isAdminLoggedIn) return;

    printf("\nSecurity Settings\n");
    printf("Current lockout duration: %d minutes\n", currentLockoutDuration);
    printf("1. Set to Normal (1 hour)\n");
    printf("2. Set to Strict (24 hours)\n");
    printf("3. Custom minutes\n");

    int choice = getValidInt("Select", 1, 3);
    switch (choice) {
        case 1: currentLockoutDuration = NORMAL_RESET_MINUTES; break;
        case 2: currentLockoutDuration = STRICT_RESET_MINUTES; break;
        case 3: 
            currentLockoutDuration = getValidInt("Enter minutes (15-1440)", 15, 1440);
            break;
    }
    saveSecuritySettings();
    printf("New lockout duration: %d minutes\n", currentLockoutDuration);
}

void getPasswordInput(char* password) {
    int i = 0;
    char ch;
    while ((ch = getch()) != '\r' && i < 49) {
        if (ch == '\b' && i > 0) {
            printf("\b \b");
            i--;
        } else if (isprint(ch)) {
            printf("*");
            password[i++] = ch;
        }
    }
    password[i] = '\0';
    printf("\n");
}

void secureErase(void* ptr, size_t size) {
    if (ptr != NULL) {
        volatile char* p = (volatile char*)ptr;
        memset(ptr, 0, size);
        for (size_t i = 0; i < size; i++) {
            p[i] = 0;
        }
        *(volatile int*)&p = 0;
    }
}

/* ========== FILE MANAGEMENT ========== */
void loadLastID() {
    FILE *fp = fopen("last_id.dat", "rb");
    if (fp) {
        if (fread(&lastMediaId, sizeof(int), 1, fp) != 1) {
            lastMediaId = 0;
        }
        fclose(fp);
    }
}

void saveLastID() {
    FILE *fp = fopen("last_id.dat", "wb");
    if (fp) {
        fwrite(&lastMediaId, sizeof(int), 1, fp);
        fclose(fp);
    } else {
        perror("Failed to save last ID");
    }
}

Media* loadLibrary() {
    FILE *fp = fopen("library.dat", "rb");
    if (!fp) return NULL;

    Media *head = NULL, *tail = NULL;
    Media temp;
    lastMediaId = 0;

    while (fread(&temp, sizeof(Media), 1, fp) == 1) {
        Media *newNode = (Media *)malloc(sizeof(Media));
        if (!newNode) {
            perror("Memory allocation failed");
            fclose(fp);
            return NULL;
        }
        *newNode = temp;
        newNode->next = NULL;

        if (strcmp(newNode->mediaType, "Watch") == 0) {
            if (newNode->watch.durationSeconds < 0 || newNode->watch.durationSeconds > 59) {
                newNode->watch.durationSeconds = 0;
            }
        }
        else if (strcmp(newNode->mediaType, "Hear") == 0) {
            if (newNode->hear.durationSeconds < 0 || newNode->hear.durationSeconds > 59) {
                newNode->hear.durationSeconds = 0;
            }
        }

        if (newNode->id > lastMediaId) {
            lastMediaId = newNode->id;
        }

        if (!head) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
    }
    
    fclose(fp);
    return head;
}

void saveLibrary() {
    FILE *fp = fopen("library.dat", "wb");
    if (!fp) {
        perror("Failed to save library");
        return;
    }

    Media *current = mediaLibrary;
    while (current) {
        if (fwrite(current, sizeof(Media), 1, fp) != 1) {
            perror("Error writing media");
            break;
        }
        current = current->next;
    }

    fclose(fp);
}

void freeLibrary() {
    Media *current = mediaLibrary;
    while (current) {
        Media *next = current->next;
        free(current);
        current = next;
    }
    mediaLibrary = NULL;
}

/* ========== CLIENT MANAGEMENT ========== */
int loadClients(Client **outClients) {
    FILE *fp = fopen("clients.dat", "rb");
    if (!fp) {
        *outClients = NULL;
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    int count = size / sizeof(Client);
    if (count == 0) {
        *outClients = NULL;
        fclose(fp);
        return 0;
    }

    *outClients = (Client*)malloc(sizeof(Client) * count);
    if (!*outClients) {
        perror("Memory allocation failed");
        fclose(fp);
        return 0;
    }

    if (fread(*outClients, sizeof(Client), (size_t)count, fp) != (size_t)count) {
        perror("Error reading clients");
        free(*outClients);
        *outClients = NULL;
        count = 0;
    }

    fclose(fp);
    return count;
}

void saveClients(Client *clients, int count) {
    if (!clients || count <= 0) return;
    
    FILE *fp = fopen("clients.dat", "wb");
    if (!fp) {
        perror("Failed to save clients");
        return;
    }

    if (fwrite(clients, sizeof(Client), (size_t)count, fp) != (size_t)count) {
        perror("Error writing clients");
    }

    fclose(fp);
}

Client* findClientById(const char *idCard) {
    if (!idCard) return NULL;

    Client *clients = NULL;
    int count = loadClients(&clients);
    Client *found = NULL;

    if (count > 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(clients[i].idCard, idCard) == 0) {
                found = (Client*)malloc(sizeof(Client));
                if (found) {
                    *found = clients[i];
                }
                break;
            }
        }
    }
    
    if (clients) free(clients);
    return found;
}

Client* createNewClient(const char *idCard) {
    if (!idCard) return NULL;

    if (!isValidIdCard(idCard)) {
        printf("Invalid ID card format. Please use AA-123456789-01 format.\n");
        return NULL;
    }

    if (findClientById(idCard)) {
        printf("Client with this ID already exists.\n");
        return NULL;
    }

    Client *clients = NULL;
    int count = loadClients(&clients);
    Client *newClient = NULL;

    Client temp;
    memset(&temp, 0, sizeof(Client));
    strncpy(temp.idCard, idCard, sizeof(temp.idCard) - 1);
    temp.lateReturnCount = 0;

    printf("New client detected.\n");
    if (!getValidString("Enter first name", temp.firstName, sizeof(temp.firstName))) {
        if (clients) free(clients);
        return NULL;
    }

    if (!getValidString("Enter last name", temp.lastName, sizeof(temp.lastName))) {
        if (clients) free(clients);
        return NULL;
    }

    Client *updatedClients = (Client*)realloc(clients, sizeof(Client) * (count + 1));
    if (!updatedClients) {
        perror("Memory allocation failed");
        if (clients) free(clients);
        return NULL;
    }
    
    clients = updatedClients;
    clients[count] = temp;
    count++;
    
    saveClients(clients, count);

    newClient = (Client*)malloc(sizeof(Client));
    if (newClient) {
        *newClient = temp;
    }

    free(clients);
    return newClient;
}

void updateClient(Client *client) {
    if (!client) return;
    
    Client *clients = NULL;
    int count = loadClients(&clients);
    int found = 0;

    if (count == 0) {
        clients = (Client*)malloc(sizeof(Client));
        if (!clients) {
            perror("Memory allocation failed");
            return;
        }
        clients[0] = *client;
        count = 1;
        found = 1;
    } else {
        for (int i = 0; i < count; i++) {
            if (strcmp(clients[i].idCard, client->idCard) == 0) {
                clients[i] = *client;
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        Client *updated = (Client*)realloc(clients, sizeof(Client) * (count + 1));
        if (!updated) {
            perror("Memory allocation failed");
            free(clients);
            return;
        }
        clients = updated;
        clients[count] = *client;
        count++;
    }

    saveClients(clients, count);
    free(clients);
}

/* ========== CLIENT MANAGEMENT FUNCTIONS ========== */
void displayClientDetails(Client *client) {
    printf("\nClient ID: %s\n", client->idCard);
    printf("Name: %s %s\n", client->firstName, client->lastName);
    printf("Late Returns: %d\n", client->lateReturnCount);

    printf("Borrowed Items:\n");
    int count = 0;
    for (int i = 0; i < MAX_BORROWED; i++) {
        if (client->borrowedItems[i].mediaId != 0) {
            Media *media = mediaLibrary;
            while (media && media->id != client->borrowedItems[i].mediaId) {
                media = media->next;
            }
            if (media) {
                printf("%d. Media ID: %d - ", i+1, media->id);
                if (strcmp(media->mediaType, "Read") == 0) {
                    printf("Book: %s\n", media->read.title);
                } else if (strcmp(media->mediaType, "Watch") == 0) {
                    printf("Movie: %s\n", media->watch.title);
                } else {
                    printf("Audio: %s\n", media->hear.title);
                }
                printf("   Borrowed on: %s", ctime(&client->borrowedItems[i].borrowDate));
                time_t now = time(NULL);
                time_t dueDate = client->borrowedItems[i].borrowDate + (BORROW_DAYS_LIMIT * 24 * 3600);
                if (now > dueDate) {
                    long daysLate = (now - dueDate) / (24 * 3600);
                    printf("   STATUS: OVERDUE by %ld days!\n", daysLate);
                }
                count++;
            }
        }
    }
    if (count == 0) {
        printf("No borrowed items\n");
    }
}

void manageClients() {
    if (!isAdminLoggedIn) {
        printf("Admin login required.\n");
        pressAnyKey();
        return;
    }

    while (1) {
        clearScreen();
        printMainTitle();
        printf("=== Client Management ===\n");
        printf("1. List All Clients\n");
        printf("2. Search Client\n");
        printf("3. View Client Details\n");
        printf("4. Block/Unblock Client\n");
        printf("0. Back to Main Menu\n");

        int choice = getValidInt("Select option", 0, 4);

        switch (choice) {
            case 1: {
                Client *clients = NULL;
                int count = loadClients(&clients);
                if (count == 0) {
                    printf("No clients found.\n");
                } else {
                    printf("\nClient List:\n");
                    for (int i = 0; i < count; i++) {
                        printf("%d. %s %s (ID: %s)\n", i+1,
                               clients[i].firstName, clients[i].lastName,
                               clients[i].idCard);
                        printf("   Late returns: %d\n", clients[i].lateReturnCount);
                    }
                }
                if (clients) free(clients);
                pressAnyKey();
                break;
            }
            case 2: {
                printf("\nSearch Options:\n");
                printf("1. By Name/ID\n");
                printf("2. By Registration Year\n");
                int searchType = getValidInt("Choose search type", 0, 2);
                if (searchType == 0) break;

                Client *clients = NULL;
                int count = loadClients(&clients);
                int found = 0;

                if (count > 0) {
                    if (searchType == 1) {
                        char searchTerm[50];
                        if (!getValidString("Enter client name or ID to search", searchTerm, sizeof(searchTerm))) {
                            if (clients) free(clients);
                            break;
                        }
                        printf("\nSearch Results:\n");
                        for (int i = 0; i < count; i++) {
                            if (strstr(clients[i].firstName, searchTerm) ||
                                strstr(clients[i].lastName, searchTerm) ||
                                strstr(clients[i].idCard, searchTerm)) {
                                printf("%d. %s %s (ID: %s)\n", found+1,
                                       clients[i].firstName, clients[i].lastName,
                                       clients[i].idCard);
                                printf("   Late returns: %d\n", clients[i].lateReturnCount);
                                found++;
                            }
                        }
                    } else {
                        int year = getValidInt("Enter registration year to search", 2000, 2100);
                        if (year == 0) {
                            if (clients) free(clients);
                            break;
                        }
                        printf("\nSearch Results:\n");
                        for (int i = 0; i < count; i++) {
                            time_t oldestDate = 0;
                            for (int j = 0; j < MAX_BORROWED; j++) {
                                if (clients[i].borrowedItems[j].borrowDate > 0 &&
                                    (oldestDate == 0 || clients[i].borrowedItems[j].borrowDate < oldestDate)) {
                                    oldestDate = clients[i].borrowedItems[j].borrowDate;
                                }
                            }
                            
                            struct tm *timeinfo = localtime(&oldestDate);
                            if (timeinfo && (timeinfo->tm_year + 1900) == year) {
                                printf("%d. %s %s (ID: %s)\n", found+1,
                                       clients[i].firstName, clients[i].lastName,
                                       clients[i].idCard);
                                printf("   Late returns: %d\n", clients[i].lateReturnCount);
                                found++;
                            }
                        }
                    }
                }

                if (found == 0 && count > 0) {
                    printf("No clients matched your search.\n");
                }

                if (clients) free(clients);
                if (found > 0 || count > 0) {
                    pressAnyKey();
                }
                break;
            }
            case 3: {
                char idCard[20];
                if (getValidString("Enter client ID card number", idCard, sizeof(idCard))) {
                    Client *client = findClientById(idCard);
                    if (client) {
                        displayClientDetails(client);
                        free(client);
                    } else {
                        printf("Client not found.\n");
                    }
                }
                pressAnyKey();
                break;
            }
            case 4: {
                char idCard[20];
                if (getValidString("Enter client ID card number to block/unblock", idCard, sizeof(idCard))) {
                    Client *client = findClientById(idCard);
                    if (client) {
                        printf("\nCurrent Status:\n");
                        displayClientDetails(client);
                        
                        if (client->lateReturnCount <= -MAX_LATE_RETURNS_ALLOWED) {
                            printf("\nClient is currently BLOCKED from borrowing.\n");
                            printf("Do you want to unblock this client? (y/n): ");
                            char confirm = getchar();
                            clearInputBuffer();
                            if (confirm == 'y' || confirm == 'Y') {
                                client->lateReturnCount = 0;
                                updateClient(client);
                                printf("Client unblocked successfully.\n");
                            }
                        } else {
                            printf("\nClient can currently borrow items.\n");
                            printf("Do you want to block this client? (y/n): ");
                            char confirm = getchar();
                            clearInputBuffer();
                            if (confirm == 'y' || confirm == 'Y') {
                                client->lateReturnCount = -MAX_LATE_RETURNS_ALLOWED;
                                updateClient(client);
                                printf("Client blocked from borrowing.\n");
                            }
                        }
                        free(client);
                    } else {
                        printf("Client not found.\n");
                    }
                }
                break;
            }
            case 0:
                return;
            default:
                printf("Invalid option.\n");
                pressAnyKey();
        }
    }
}

/* ========== ADMIN LOGIN ========== */
AdminCredentials loadAdminCredentials() {
    AdminCredentials creds = {"admin", "password123", "Default password"};
    FILE* file = fopen(CREDENTIALS_FILE, "rb");
    if (file) {
        fread(&creds, sizeof(AdminCredentials), 1, file);
        fclose(file);
    }
    return creds;
}

void saveAdminCredentials(AdminCredentials creds) {
    FILE* file = fopen(CREDENTIALS_FILE, "wb");
    if (file) {
        fwrite(&creds, sizeof(AdminCredentials), 1, file);
        fclose(file);
    }
}

void changeAdminCredentials() {
    if (!isAdminLoggedIn) {
        printf("\nAccess denied. Admin login required.\n");
        pressAnyKey();
        return;
    }

    AdminCredentials current = loadAdminCredentials();
    AdminCredentials newCreds;

    printf("\n=== CHANGE ADMIN CREDENTIALS ===\n");

    // Verify identity
    char currentPassword[50];
    printf("Current password: ");
    getPasswordInput(currentPassword);
    
    if (strcmp(currentPassword, current.password) != 0) {
        printf("\nError: Incorrect password.\n");
        pressAnyKey();
        return;
    }
    
    // New username
    printf("\nNew username (current: %s): ", current.username);
    if (!getValidString("", newCreds.username, sizeof(newCreds.username))) {
        printf("Username change cancelled.\n");
        pressAnyKey();
        return;
    }
    
    // New password
    printf("New password: ");
    getPasswordInput(newCreds.password);
    
    // Check if password is empty
    if (strlen(newCreds.password) == 0) {
        printf("Password cannot be empty. Changes cancelled.\n");
        pressAnyKey();
        return;
    }
    
    // Confirmation
    char confirmPassword[50];
    printf("Confirm password: ");
    getPasswordInput(confirmPassword);

    if (strcmp(newCreds.password, confirmPassword) != 0) {
        printf("\nError: Passwords don't match. Changes cancelled.\n");
        pressAnyKey();
        return;
    }

    // Password hint
    printf("Password hint (optional): ");
    fgets(newCreds.passwordHint, sizeof(newCreds.passwordHint), stdin);
    newCreds.passwordHint[strcspn(newCreds.passwordHint, "\n")] = 0;
    
    // Save
    saveAdminCredentials(newCreds);
    printf("\nCredentials updated successfully!\n");
    printf("Hint: %s\n", newCreds.passwordHint);
    secureErase(currentPassword, sizeof(currentPassword));
    secureErase(&newCreds, sizeof(newCreds));
    pressAnyKey();
}
int adminLogin() {
    time_t now = time(NULL);
    loadSecuritySettings();

    AdminCredentials creds = loadAdminCredentials(); 

    // Check if account is permanently locked
    if (permanentLockoutEndTime > 0 && now < permanentLockoutEndTime) { 
        printf("Account permanently locked until %s", ctime(&permanentLockoutEndTime)); 
        pressAnyKey(); 
        return 0;
    }

    // Check if in temporary lockout period
    if (currentLoginAttempts >= MAX_LOGIN_ATTEMPTS) { 
        // LIGNE À MODIFIER POUR CHANGER LA PROGRESSION DU BLOCAGE TEMPORAIRE
        int lockoutDuration = 60 * (1 << (totalFailedLogins/MAX_LOGIN_ATTEMPTS - 1)); 
        if (now < temporaryLockoutTime + lockoutDuration) { 
            printf("Try again in %ld seconds\n", (long)((temporaryLockoutTime + lockoutDuration) - now));
            pressAnyKey();
            return 0;
        }
        currentLoginAttempts = 0; // Reset temporary lockout counter 
    }

    // Get credentials
    char username[20], password[20];
    printf("Admin Login (Attempts: %d/%d)\n", totalFailedLogins, MAX_FAILED_ATTEMPTS); 
    if (totalFailedLogins > 0) { 
        long remaining = (currentLockoutDuration * 60) - (now - firstFailedLoginTime); 
        printf("Reset in: %02ld:%02ld\n", remaining/60, remaining%60); 
    }
    printf("Username: "); 
    if (!fgets(username, sizeof(username), stdin)) { 
        clearInputBuffer();
        return 0; 
    }

    username[strcspn(username, "\n")] = '\0';

    printf("Password: ");
    int i = 0;
    while (1) {
        char ch = getch();
        if (ch == '\r' || ch == '\n') {
            password[i] = '\0';
            break;
        } else if (ch == '\b' && i > 0) {
            printf("\b \b");
            i--;
        } else if ((size_t)i < sizeof(password) - 1) {
            password[i++] = ch;
            printf("*");
        }
    }
     printf("\n"); 

    // Check credentials
    if (strcmp(username, creds.username) == 0 && strcmp(password, creds.password) == 0) {
        // Login successful 
        isAdminLoggedIn = 1; 
        currentLoginAttempts = 0; 
        totalFailedLogins = 0; 
        printf("Login successful!\n"); 
        return 1; 
    } else {
        // Login failed
        currentLoginAttempts++; 
        if (totalFailedLogins == 0) { 
            firstFailedLoginTime = now; 
        }
        totalFailedLogins++; 

        // Check for permanent lockout 
        if (totalFailedLogins >= MAX_FAILED_ATTEMPTS) {
            permanentLockoutEndTime = now + 86400; // 24-hour lock 
            printf("PERMANENT LOCKOUT ACTIVATED\n"); 
        }
        // Check for temporary lockout 
        else if (currentLoginAttempts >= MAX_LOGIN_ATTEMPTS) { 
            temporaryLockoutTime = now; 
            printf("Temporary lockout activated\n"); 
        }

        saveSecuritySettings(); 
        printf("Attempts: %d/%d\n", totalFailedLogins, MAX_FAILED_ATTEMPTS); 
        secureErase(password, sizeof(password)); 
        pressAnyKey(); 
        return 0; 
    }
}

/* ========== MEDIA MANAGEMENT ========== */
void displayMediaDetails(Media *media, int showPrompt) {
    if (!media) return;

    printf("\nMedia ID: %d\n", media->id);
    if (strcmp(media->mediaType, "Read") == 0) {
        printf("[Reading Material]\n");
        printf("Title: %s\nAuthor: %s\nCategory: %s\nPages: %d\nPublisher: %s\nYear: %d\n",
              media->read.title, media->read.author, media->read.category,
              media->read.pages, media->read.publisher, media->read.year);
        printf("Type: %s\n", (media->read.type == BOOK) ? "Book" :
                            (media->read.type == MAGAZINE) ? "Magazine" : "Newspaper");
        printf("Status: %s\n", media->read.isBorrowed ? "Borrowed" : "Available");
        if (media->read.isBorrowed) {
            printf("Borrower: %s\n", media->read.borrower);
            printf("Borrowed on: %s", ctime(&media->read.borrowDate));
        }
    } else if (strcmp(media->mediaType, "Watch") == 0) {
        printf("[Watching Material]\n");
        printf("Title: %s\nDirector: %s\nCategory: %s\n",
              media->watch.title, media->watch.director, media->watch.category);
        printf("Duration: ");
        printDuration(media->watch.durationMinutes, media->watch.durationSeconds);
        printf("\nRelease Date: %s\n", media->watch.releaseDate);
        printf("Type: %s\n", (media->watch.type == MOVIE) ? "Movie" :
                            (media->watch.type == TV_SHOW) ? "TV Show" : "Documentary");
        printf("Status: %s\n", media->watch.isBorrowed ? "Borrowed" : "Available");
        if (media->watch.isBorrowed) {
            printf("Borrower: %s\n", media->watch.borrower);
            printf("Borrowed on: %s", ctime(&media->watch.borrowDate));
        }
    } else if (strcmp(media->mediaType, "Hear") == 0) {
        printf("[Listening Material]\n");
        printf("Title: %s\nArtist: %s\nCategory: %s\n",
              media->hear.title, media->hear.artist, media->hear.category);
        printf("Duration: ");
        printDuration(media->hear.durationMinutes, media->hear.durationSeconds);
        printf("\nYear: %d\n", media->hear.year);
        printf("Type: %s\n", (media->hear.type == MUSIC) ? "Music" :
                            (media->hear.type == PODCAST) ? "Podcast" : "Audiobook");
        printf("Status: %s\n", media->hear.isBorrowed ? "Borrowed" : "Available");
        if (media->hear.isBorrowed) {
            printf("Borrower: %s\n", media->hear.borrower);
            printf("Borrowed on: %s", ctime(&media->hear.borrowDate));
        }
    }
    if (showPrompt) {
        pressAnyKey();
    }
}

int isDuplicateMedia(Media *newMedia) {
    Media *current = mediaLibrary;
    while (current) {
        if (strcmp(current->mediaType, newMedia->mediaType) == 0) {
            if (strcmp(newMedia->mediaType, "Read") == 0) {
                if (caseInsensitiveCompare(current->read.title, newMedia->read.title) == 0 &&
                    caseInsensitiveCompare(current->read.author, newMedia->read.author) == 0) {
                    return 1;
                }
            } else if (strcmp(newMedia->mediaType, "Watch") == 0) {
                if (caseInsensitiveCompare(current->watch.title, newMedia->watch.title) == 0 &&
                    caseInsensitiveCompare(current->watch.director, newMedia->watch.director) == 0) {
                    return 1;
                }
            } else if (strcmp(newMedia->mediaType, "Hear") == 0) {
                if (caseInsensitiveCompare(current->hear.title, newMedia->hear.title) == 0 &&
                    caseInsensitiveCompare(current->hear.artist, newMedia->hear.artist) == 0) {
                    return 1;
                }
            }
        }
        current = current->next;
    }
    return 0;
}

void addMedia() {
    if (!isAdminLoggedIn) {
        printf("Admin login required.\n");
        pressAnyKey();
        return;
    }

    clearScreen();
    printMainTitle();
    printf("=== Add New Media ===\n");
    printf("Select media type:\n1. Reading Material\n2. Watching Material\n3. Listening Material\n0. Back\n");

    int choice = getValidInt("Choice", 0, 3);
    if (choice == 0) return;

    Media *newMedia = (Media*)malloc(sizeof(Media));
    if (!newMedia) {
        printf("Memory allocation failed.\n");
        pressAnyKey();
        return;
    }
    memset(newMedia, 0, sizeof(Media));
    newMedia->next = NULL;

    switch (choice) {
        case 1: {
            strcpy(newMedia->mediaType, "Read");
            if (!getValidString("Enter title", newMedia->read.title, sizeof(newMedia->read.title))) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter author", newMedia->read.author, sizeof(newMedia->read.author))) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter category", newMedia->read.category, sizeof(newMedia->read.category))) {
                free(newMedia);
                return;
            }
            newMedia->read.pages = getValidInt("Enter pages", 1, 10000);
            if (newMedia->read.pages == 0) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter publisher", newMedia->read.publisher, sizeof(newMedia->read.publisher))) {
                free(newMedia);
                return;
            }
            newMedia->read.year = getValidInt("Enter year", 1900, 2100);
            if (newMedia->read.year == 0) {
                free(newMedia);
                return;
            }
            printf("Reading types:\n1. Book\n2. Magazine\n3. Newspaper\n");
            int rtype = getValidInt("Select type", 1, 3);
            if (rtype == 0) {
                free(newMedia);
                return;
            }
            newMedia->read.type = (ReadType)(rtype - 1);
            break;
        }
        case 2: {
            strcpy(newMedia->mediaType, "Watch");
            if (!getValidString("Enter title", newMedia->watch.title, sizeof(newMedia->watch.title))) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter director", newMedia->watch.director, sizeof(newMedia->watch.director))) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter category", newMedia->watch.category, sizeof(newMedia->watch.category))) {
                free(newMedia);
                return;
            }
            getDurationInput(&newMedia->watch.durationMinutes, &newMedia->watch.durationSeconds);
            if (newMedia->watch.durationMinutes == 0 && newMedia->watch.durationSeconds == 0) {
                free(newMedia);
                return;
            }
            if (!getValidDate("Enter release date", newMedia->watch.releaseDate,
                               sizeof(newMedia->watch.releaseDate))) {
                free(newMedia);
                return;
            }
            printf("Watch types:\n1. Movie\n2. TV Show\n3. Documentary\n");
            int wtype = getValidInt("Select type", 1, 3);
            if (wtype == 0) {
                free(newMedia);
                return;
            }
            newMedia->watch.type = (WatchType)(wtype - 1);
            break;
        }
        case 3: {
            strcpy(newMedia->mediaType, "Hear");
            if (!getValidString("Enter title", newMedia->hear.title, sizeof(newMedia->hear.title))) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter artist", newMedia->hear.artist, sizeof(newMedia->hear.artist))) {
                free(newMedia);
                return;
            }
            if (!getValidString("Enter category", newMedia->hear.category, sizeof(newMedia->hear.category))) {
                free(newMedia);
                return;
            }
            getDurationInput(&newMedia->hear.durationMinutes, &newMedia->hear.durationSeconds);
            if (newMedia->hear.durationMinutes == 0 && newMedia->hear.durationSeconds == 0) {
                free(newMedia);
                return;
            }
            newMedia->hear.year = getValidInt("Enter year", 1900, 2100);
            if (newMedia->hear.year == 0) {
                free(newMedia);
                return;
            }
            printf("Hear types:\n1. Music\n2. Podcast\n3. Audiobook\n");
            int htype = getValidInt("Select type", 1, 3);
            if (htype == 0) {
                free(newMedia);
                return;
            }
            newMedia->hear.type = (HearType)(htype - 1);
            break;
        }
        default:
            free(newMedia);
            return;
    }

    if (isDuplicateMedia(newMedia)) {
        printf("Duplicate media detected. Cannot add.\n");
        free(newMedia);
        pressAnyKey();
        return;
    }

    lastMediaId++;
    newMedia->id = lastMediaId;

    if (newMedia->id > lastMediaId) {
        lastMediaId = newMedia->id;
    }

    if (!mediaLibrary) {
        mediaLibrary = newMedia;
    } else {
        Media *current = mediaLibrary;
        while (current->next) current = current->next;
        current->next = newMedia;
    }

    saveLibrary();
    saveLastID();

    printf("Media added successfully with ID %d.\n", newMedia->id);
    pressAnyKey();
}

void displayMedia() {
    if (!mediaLibrary) {
        printf("No media available.\n");
        pressAnyKey();
        return;
    }

    Media *current = mediaLibrary;
    int count = 0;

    clearScreen();
    printMainTitle();
    printf("=== All Media Items ===\n\n");

    while (current) {
        printf("Media ID: %d\n", current->id);
        if (strcmp(current->mediaType, "Read") == 0) {
            printf("[Reading Material]\n");
            printf("Title: %s\nAuthor: %s\nCategory: %s\nPages: %d\nPublisher: %s\nYear: %d\n",
                  current->read.title, current->read.author, current->read.category,
                  current->read.pages, current->read.publisher, current->read.year);
            printf("Type: %s\n", (current->read.type == BOOK) ? "Book" :
                                (current->read.type == MAGAZINE) ? "Magazine" : "Newspaper");
            printf("Status: %s\n", current->read.isBorrowed ? "Borrowed" : "Available");
            if (current->read.isBorrowed && isAdminLoggedIn) {
                printf("Borrower: %s\n", current->read.borrower);
                printf("Borrowed on: %s", ctime(&current->read.borrowDate));
            }
        } else if (strcmp(current->mediaType, "Watch") == 0) {
            printf("[Watching Material]\n");
            printf("Title: %s\nDirector: %s\nCategory: %s\n",
                  current->watch.title, current->watch.director, current->watch.category);
            printf("Duration: ");
            printDuration(current->watch.durationMinutes, current->watch.durationSeconds);
            printf("\nRelease Date: %s\n", current->watch.releaseDate);
            printf("Type: %s\n", (current->watch.type == MOVIE) ? "Movie" :
                                (current->watch.type == TV_SHOW) ? "TV Show" : "Documentary");
            printf("Status: %s\n", current->watch.isBorrowed ? "Borrowed" : "Available");
            if (current->watch.isBorrowed && isAdminLoggedIn) {
                printf("Borrower: %s\n", current->watch.borrower);
                printf("Borrowed on: %s", ctime(&current->watch.borrowDate));
            }
        } else if (strcmp(current->mediaType, "Hear") == 0) {
            printf("[Listening Material]\n");
            printf("Title: %s\nArtist: %s\nCategory: %s\n",
                  current->hear.title, current->hear.artist, current->hear.category);
            printf("Duration: ");
            printDuration(current->hear.durationMinutes, current->hear.durationSeconds);
            printf("\nYear: %d\n", current->hear.year);
            printf("Type: %s\n", (current->hear.type == MUSIC) ? "Music" :
                                (current->hear.type == PODCAST) ? "Podcast" : "Audiobook");
            printf("Status: %s\n", current->hear.isBorrowed ? "Borrowed" : "Available");
            if (current->hear.isBorrowed && isAdminLoggedIn) {
                printf("Borrower: %s\n", current->hear.borrower);
                printf("Borrowed on: %s", ctime(&current->hear.borrowDate));
            }
        }
        printf("---------------------------\n");
        current = current->next;
        count++;
    }

    printf("\nTotal media items: %d\n", count);
    pressAnyKey();
}

int searchAndSelectMedia(int allowBack) {
    if (!mediaLibrary) {
        printf("No media to search.\n");
        pressAnyKey();
        return 0;
    }

    while (1) {
        clearScreen();
        printMainTitle();
        printf("=== Media Search ===\n");
        printf("1. By Title\n2. By Creator (Author/Director/Artist)\n3. By Category\n4. By Year\n");
        if (allowBack) printf("0. Back\n");

        int choice = getValidInt("Search option", 0, 4);
        if (allowBack && choice == 0) return 0;
        if (choice < 1 || choice > 4) continue;

        char searchTerm[MAX_INPUT_LENGTH];
        int year = 0;
        if (choice != 4) {
            if (!getValidString("Enter search term", searchTerm, sizeof(searchTerm))) {
                continue;
            }
        } else {
            year = getValidInt("Enter publication year to search", 1900, 2100);
            if (year == 0) continue;
        }

        Media *results[MAX_MEDIA];
        int resultCount = 0;
        Media *current = mediaLibrary;

        while (current && resultCount < MAX_MEDIA) {
            int match = 0;
            if (choice == 1) {
                if (strcmp(current->mediaType, "Read") == 0 && 
                    caseInsensitiveStrStr(current->read.title, searchTerm)) {
                    match = 1;
                } else if (strcmp(current->mediaType, "Watch") == 0 && 
                          caseInsensitiveStrStr(current->watch.title, searchTerm)) {
                    match = 1;
                } else if (strcmp(current->mediaType, "Hear") == 0 && 
                          caseInsensitiveStrStr(current->hear.title, searchTerm)) {
                    match = 1;
                }
            } else if (choice == 2) {
                if (strcmp(current->mediaType, "Read") == 0 &&
                    caseInsensitiveStrStr(current->read.author, searchTerm)) {
                    match = 1;
                } else if (strcmp(current->mediaType, "Watch") == 0 && 
                          caseInsensitiveStrStr(current->watch.director, searchTerm)) {
                    match = 1;
                } else if (strcmp(current->mediaType, "Hear") == 0 &&
                          caseInsensitiveStrStr(current->hear.artist, searchTerm)) {
                    match = 1;
                }
            } else if (choice == 3) {
                if (strcmp(current->mediaType, "Read") == 0 &&
                    caseInsensitiveStrStr(current->read.category, searchTerm)) {
                    match = 1;
                } else if (strcmp(current->mediaType, "Watch") == 0 &&
                          caseInsensitiveStrStr(current->watch.category, searchTerm)) {
                    match = 1;
                } else if (strcmp(current->mediaType, "Hear") == 0 &&
                          caseInsensitiveStrStr(current->hear.category, searchTerm)) {
                    match = 1;
                }
            } else if (choice == 4) {
                int mediaYear = 0;
                if (strcmp(current->mediaType, "Read") == 0) {
                    mediaYear = current->read.year;
                } else if (strcmp(current->mediaType, "Watch") == 0) {
                    if (strlen(current->watch.releaseDate) >= 4) {
                        char yearStr[5];
                        strncpy(yearStr, current->watch.releaseDate, 4);
                        yearStr[4] = '\0';
                        mediaYear = atoi(yearStr);
                    }
                } else if (strcmp(current->mediaType, "Hear") == 0) {
                    mediaYear = current->hear.year;
                }
                if (mediaYear == year) {
                    match = 1;
                }
            }

            if (match) {
                results[resultCount++] = current;
            }
            current = current->next;
        }

        if (resultCount == 0) {
            printf("No media matched your search.\n");
            pressAnyKey();
            continue;
        }

        printf("\nSearch Results (%d found):\n", resultCount);
        for (int i = 0; i < resultCount; i++) {
            printf("%d. ID %d - ", i + 1, results[i]->id);
            if (strcmp(results[i]->mediaType, "Read") == 0) {
                printf("[Read] %s by %s (%d)\n", results[i]->read.title, results[i]->read.author, results[i]->read.year);
            } else if (strcmp(results[i]->mediaType, "Watch") == 0) {
                char yearStr[5] = {0};
                if (strlen(results[i]->watch.releaseDate) >= 4) {
                    strncpy(yearStr, results[i]->watch.releaseDate, 4);
                }
                printf("[Watch] %s by %s (%s)\n", results[i]->watch.title, results[i]->watch.director, yearStr);
            } else {
                printf("[Hear] %s by %s (%d)\n", results[i]->hear.title, results[i]->hear.artist, results[i]->hear.year);
            }
        }
        printf("0. Back\n");

        int selection = getValidInt("Select media number to view details", 1, resultCount);
        if (selection == 0) continue;

        displayMediaDetails(results[selection - 1], 1);

        return results[selection - 1]->id;
    }
}

void borrowMedia() {
    if (!mediaLibrary) {
        printf("No media available to borrow.\n");
        pressAnyKey();
        return;
    }

    int mediaId = searchAndSelectMedia(1);
    if (mediaId == 0) return;

    Media *current = mediaLibrary;
    while (current && current->id != mediaId) current = current->next;

    if (!current) {
        printf("\nMedia not found.\n");
        pressAnyKey();
        return;
    }

    if (isMediaBorrowed(current)) {
        printf("\nSorry, this media is currently lent out.\n");
        pressAnyKey();
        return;
    }

    char idCard[20];
    if (!getValidId("\nEnter your ID card number", idCard, sizeof(idCard))) {
        return;
    }

    Client *client = findClientById(idCard);
    if (!client) {
        client = createNewClient(idCard);
        if (!client) {
            printf("\nFailed to create client.\n");
            pressAnyKey();
            return;
        }
    }

    if (client->lateReturnCount <= -MAX_LATE_RETURNS_ALLOWED) {
        printf("\nYou are blocked from borrowing due to excessive late returns.\n");
        free(client);
        pressAnyKey();
        return;
    }

    int availableSlot = -1;
    for (int i = 0; i < MAX_BORROWED; i++) {
        if (client->borrowedItems[i].mediaId == 0) {
            availableSlot = i;
            break;
        }
    }

    if (availableSlot == -1) {
        printf("\nYou have reached the maximum borrowed items (%d).\n", MAX_BORROWED);
        free(client);
        pressAnyKey();
        return;
    }

    client->borrowedItems[availableSlot].mediaId = mediaId;
    client->borrowedItems[availableSlot].borrowDate = time(NULL);

    setMediaBorrowedStatus(current, 1, client->idCard);
    updateClient(client);
    saveLibrary();

    printf("\nMedia borrowed successfully!\n");
    
    time_t dueDate = client->borrowedItems[availableSlot].borrowDate + (BORROW_DAYS_LIMIT * 24 * 3600);

    char dueDateStr[26];
    struct tm *timeInfo = localtime(&dueDate);
    strftime(dueDateStr, sizeof(dueDateStr), "%Y-%m-%d %H:%M:%S", timeInfo);

    printf("Due date: %s\n", dueDateStr);
    printf("Please return on time to avoid penalties.\n");

    free(client);
    pressAnyKey();
}

void returnMedia() {
    char idCard[20];
    if (!getValidId("Enter your ID card number", idCard, sizeof(idCard))) {
        return;
    }

    Client *client = findClientById(idCard);
    if (!client) {
        printf("Client not found.\n");
        pressAnyKey();
        return;
    }

    int borrowedCount = 0;
    printf("\nYour Borrowed Media:\n");
    for (int i = 0; i < MAX_BORROWED; i++) {
        if (client->borrowedItems[i].mediaId != 0) {
            Media *current = mediaLibrary;
            while (current) {
                if (current->id == client->borrowedItems[i].mediaId) break;
                current = current->next;
            }

            if (current) {
                printf("%d. ID: %d - ", i + 1, current->id);
                if (strcmp(current->mediaType, "Read") == 0) {
                    printf("%s by %s", current->read.title, current->read.author);
                } else if (strcmp(current->mediaType, "Watch") == 0) {
                    printf("%s by %s", current->watch.title, current->watch.director);
                } else {
                    printf("%s by %s", current->hear.title, current->hear.artist);
                }

                time_t dueDate = client->borrowedItems[i].borrowDate + (BORROW_DAYS_LIMIT * 24 * 3600);
                time_t now = time(NULL);

                if (now > dueDate) {
                    printf(" (OVERDUE by %ld days)", (long)((now - dueDate) / (24 * 3600)));
                }
                printf("\n");
                borrowedCount++;
            }
        }
    }

    if (borrowedCount == 0) {
        printf("You have no borrowed media.\n");
        free(client);
        pressAnyKey();
        return;
    }

    int mediaId = getValidInt("Enter media ID to return", 1, lastMediaId);
    if (mediaId == 0) {
        free(client);
        return;
    }

    int foundIndex = -1;
    for (int i = 0; i < MAX_BORROWED; i++) {
        if (client->borrowedItems[i].mediaId == mediaId) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex == -1) {
        printf("You have not borrowed media with ID %d.\n", mediaId);
        free(client);
        pressAnyKey();
        return;
    }

    Media *current = mediaLibrary;
    while (current) {
        if (current->id == mediaId) break;
        current = current->next;
    }

    if (!current) {
        printf("Media not found in library.\n");
        free(client);
        pressAnyKey();
        return;
    }

    time_t now = time(NULL);
    time_t dueDate = client->borrowedItems[foundIndex].borrowDate + (BORROW_DAYS_LIMIT * 24 * 3600);

    if (now > dueDate) {
        long daysLate = (now - dueDate) / (24 * 3600);
        client->lateReturnCount--;
        printf("\nYou returned the media %ld days late! Late returns now: %d\n",
              daysLate, client->lateReturnCount);
    }
    client->borrowedItems[foundIndex].returnDate = now;
    client->borrowedItems[foundIndex].mediaId = 0;

    setMediaBorrowedStatus(current, 0, NULL);

    updateClient(client);
    saveLibrary();

    printf("Media returned successfully.\n");
    free(client);
    pressAnyKey();
}

void modifyMedia() {
    if (!isAdminLoggedIn) {
        printf("Admin login required.\n");
        pressAnyKey();
        return;
    }

    if (!mediaLibrary) {
        printf("No media available to modify.\n");
        pressAnyKey();
        return;
    }

    int id = getMediaIdWithOptions("modify");
    if (id == 0) return;

    Media *current = mediaLibrary;
    while (current) {
        if (current->id == id) break;
        current = current->next;
    }
    
    if (!current) {
        printf("Media not found.\n");
        pressAnyKey();
        return;
    }
    displayMediaDetails(current, 0);
    printf("\nSelect field to modify (or 0 to cancel):\n");

    if (strcmp(current->mediaType, "Read") == 0) {
        printf("1. Title\n2. Author\n3. Category\n4. Pages\n5. Publisher\n6. Year\n7. Type\n");
        int field = getValidInt("Field", 0, 7);
        if (field == 0) return;

        char buffer[50];
        int ivalue;

        switch (field) {
            case 1:
                if (getValidString("New title", buffer, sizeof(buffer))) {
                    strncpy(current->read.title, buffer, sizeof(current->read.title)-1);
                }
                break;
            case 2:
                if (getValidString("New author", buffer, sizeof(buffer))) {
                    strncpy(current->read.author, buffer, sizeof(current->read.author)-1);
                }
                break;
            case 3:
                if (getValidString("New category", buffer, sizeof(buffer))) {
                    strncpy(current->read.category, buffer, sizeof(current->read.category)-1);
                }
                break;
            case 4:
                ivalue = getValidInt("New pages", 1, 10000);
                if (ivalue > 0) current->read.pages = ivalue;
                break;
            case 5:
                if (getValidString("New publisher", buffer, sizeof(buffer))) {
                    strncpy(current->read.publisher, buffer, sizeof(current->read.publisher)-1);
                }
                break;
            case 6:
                ivalue = getValidInt("New year", 1900, 2100);
                if (ivalue > 0) current->read.year = ivalue;
                break;
            case 7:
                printf("Reading types:\n1. Book\n2. Magazine\n3. Newspaper\n");
                ivalue = getValidInt("New type", 1, 3);
                if (ivalue > 0) current->read.type = (ReadType)(ivalue - 1);
                break;
        }
    }
    else if (strcmp(current->mediaType, "Watch") == 0) {
        printf("1. Title\n2. Director\n3. Category\n4. Duration\n5. Release Date\n6. Type\n");
        int field = getValidInt("Field", 0, 6);
        if (field == 0) return;

        char buffer[50];
        int ivalue;

        switch (field) {
            case 1:
                if (getValidString("New title", buffer, sizeof(buffer))) {
                    strncpy(current->watch.title, buffer, sizeof(current->watch.title)-1);
                }
                break;
            case 2:
                if (getValidString("New director", buffer, sizeof(buffer))) {
                    strncpy(current->watch.director, buffer, sizeof(current->watch.director)-1);
                }
                break;
            case 3:
                if (getValidString("New category", buffer, sizeof(buffer))) {
                    strncpy(current->watch.category, buffer, sizeof(current->watch.category)-1);
                }
                break;
            case 4: {
                int mins, secs;
                getDurationInput(&mins, &secs);
                current->watch.durationMinutes = mins;
                current->watch.durationSeconds = secs;
                break;
            }
            case 5: {
                char newDate[20];
                if (getValidDate("Enter new release date", newDate, sizeof(newDate))) {
                    strncpy(current->watch.releaseDate, newDate, sizeof(current->watch.releaseDate)-1);
                }
                break;
            }
            case 6:
                printf("Watch types:\n1. Movie\n2. TV Show\n3. Documentary\n");
                ivalue = getValidInt("New type", 1, 3);
                if (ivalue > 0) current->watch.type = (WatchType)(ivalue - 1);
                break;
        }
    } 
    else if (strcmp(current->mediaType, "Hear") == 0) {
        printf("1. Title\n2. Artist\n3. Category\n4. Duration\n5. Year\n6. Type\n");
        int field = getValidInt("Field", 0, 6);
        if (field == 0) return;

        char buffer[50];
        int ivalue;

        switch (field) {
            case 1:
                if (getValidString("New title", buffer, sizeof(buffer))) {
                    strncpy(current->hear.title, buffer, sizeof(current->hear.title)-1);
                }
                break;
            case 2:
                if (getValidString("New artist", buffer, sizeof(buffer))) {
                    strncpy(current->hear.artist, buffer, sizeof(current->hear.artist)-1);
                }
                break;
            case 3:
                if (getValidString("New category", buffer, sizeof(buffer))) {
                    strncpy(current->hear.category, buffer, sizeof(current->hear.category)-1);
                }
                break;
            case 4: {
                int mins, secs;
                getDurationInput(&mins, &secs);
                current->hear.durationMinutes = mins;
                current->hear.durationSeconds = secs;
                break;
            }
            case 5: {
                int year;
                do {
                    year = getValidInt("Enter year (1900-2100)", 1900, 2100);
                    if (year == 0) break;
                    if (year >= 1900 && year <= 2100) {
                        current->hear.year = year;
                        break;
                    }
                    printf("Year must be between 1900-2100.\n");
                } while (1);
                break;
            }
            case 6:
                printf("Hear types:\n1. Music\n2. Podcast\n3. Audiobook\n");
                ivalue = getValidInt("New type", 1, 3);
                if (ivalue > 0) current->hear.type = (HearType)(ivalue - 1);
                break;
        }
    }

    saveLibrary();
    printf("Media modified successfully.\n");
    pressAnyKey();
}

void deleteMedia() {
    if (!isAdminLoggedIn) {
        printf("Admin login required.\n");
        pressAnyKey();
        return;
    }

    if (!mediaLibrary) {
        printf("No media available to delete.\n");
        pressAnyKey();
        return;
    }

    int id = getMediaIdWithOptions("delete");
    if (id == 0) return;

    Media *current = mediaLibrary;
    Media *prev = NULL;

    while (current) {
        if (current->id == id) break;
        prev = current;
        current = current->next;
    }

    if (!current) {
        printf("Media not found.\n");
        pressAnyKey();
        return;
    }

    if (isMediaBorrowed(current)) {
        printf("Cannot delete media that is currently borrowed.\n");
        pressAnyKey();
        return;
    }

    displayMediaDetails(current, 0);
    printf("Are you sure you want to delete this media? (y/n): ");
    char confirm;
    scanf(" %c", &confirm);
    clearInputBuffer();

    if (confirm != 'y' && confirm != 'Y') {
        printf("Deletion cancelled.\n");
        pressAnyKey();
        return;
    }

    if (prev == NULL) {
        mediaLibrary = current->next;
    } else {
        prev->next = current->next;
    }
    
    free(current);
    saveLibrary();
    printf("Media deleted successfully.\n");
    pressAnyKey();
}

/* ========== MAIN MENU ========== */
void mainMenu() {
    while (1) {
        clearScreen();
        printMainTitle();
        
        printf("1. Display All Media\n");
        printf("2. Search Media\n");
        printf("3. Borrow Media\n");
        printf("4. Return Media\n");

        if (isAdminLoggedIn) {
            printf("5. Add Media\n");
            printf("6. Modify Media\n");
            printf("7. Delete Media\n");
            printf("8. Manage Clients\n");
            printf("9. Security Settings\n");
            printf("10. Change Admin Credentials\n");
            printf("11. Logout\n");
            printf("0. Exit\n");
        } else {
            printf("5. Admin Login\n");
            printf("0. Exit\n");
        }

        int maxOption = isAdminLoggedIn ? 11 : 5;
        int choice = getValidInt("Select option", 0, maxOption);

        switch (choice) {
            case 1: displayMedia(); break;
            case 2: searchAndSelectMedia(1); break;
            case 3: borrowMedia(); break;
            case 4: returnMedia(); break;
            case 5:
                if (isAdminLoggedIn) {
                    addMedia();
                } else {
                    adminLogin();
                }
                break;
            case 6:
                if (isAdminLoggedIn) modifyMedia();
                else {
                    printf("Invalid option.\n");
                    pressAnyKey();
                }
                break;
            case 7:
                if (isAdminLoggedIn) deleteMedia();
                else {
                    printf("Invalid option.\n");
                    pressAnyKey();
                }
                break;
            case 8:
                if (isAdminLoggedIn) manageClients();
                else {
                    printf("Invalid option.\n");
                    pressAnyKey();
                }
                break;
            case 9: 
                if (isAdminLoggedIn) configureSecuritySettings();
                else {
                    printf("Invalid option.\n");
                    pressAnyKey();
                }
                break;
            case 10:
                if (isAdminLoggedIn) changeAdminCredentials();
                else {
                    printf("Invalid option.\n");
                    pressAnyKey();
                }
                break;
            case 11:
                if (isAdminLoggedIn) {
                    isAdminLoggedIn = 0;
                    printf("Logged out successfully.\n");
                    pressAnyKey();
                } else {
                    printf("Invalid option.\n");
                    pressAnyKey();
                }
                break;
            case 0:
                printf("Exiting...\n");
                return;
            default:
                printf("Invalid option.\n");
                pressAnyKey();
        }
    }
}

/* ========== MAIN ========== */
int main() {
    loadLastID();
    mediaLibrary = loadLibrary();

    if (!mediaLibrary) {
        printf("Initializing empty media library...\n");
    }

    mainMenu();

    saveLastID();
    saveLibrary();
    freeLibrary();

    return 0;
}
