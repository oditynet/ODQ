#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void generate_macro(const char* filename, int num_records) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error creating macro file\n");
        return;
    }

    // Списки для генерации разнообразных данных
    const char* first_names[] = {
        "John", "Alice", "Bob", "Emma", "Michael", "Sophia", "William", "Olivia",
        "James", "Isabella", "Benjamin", "Mia", "Lucas", "Charlotte", "Henry", "Amelia",
        "Alexander", "Harper", "Daniel", "Evelyn", "Matthew", "Abigail", "David", "Emily",
        "Joseph", "Elizabeth", "Samuel", "Sofia", "Jackson", "Avery", "Sebastian", "Ella"
    };

    const char* last_names[] = {
        "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller", "Davis",
        "Rodriguez", "Martinez", "Hernandez", "Lopez", "Gonzalez", "Wilson", "Anderson",
        "Thomas", "Taylor", "Moore", "Jackson", "Martin", "Lee", "Perez", "Thompson",
        "White", "Harris", "Sanchez", "Clark", "Ramirez", "Lewis", "Robinson", "Walker",
        "Young", "Allen", "King", "Wright", "Scott", "Torres", "Nguyen", "Hill", "Flores"
    };

    const char* domains[] = {
        "gmail.com", "yahoo.com", "hotmail.com", "outlook.com", "protonmail.com",
        "icloud.com", "aol.com", "zoho.com", "yandex.com", "mail.com"
    };

    const char* cities[] = {
        "New York", "Los Angeles", "Chicago", "Houston", "Phoenix", "Philadelphia",
        "San Antonio", "San Diego", "Dallas", "San Jose", "Austin", "Jacksonville",
        "Fort Worth", "Columbus", "Indianapolis", "Charlotte", "San Francisco", "Seattle",
        "Denver", "Washington", "Boston", "El Paso", "Nashville", "Detroit", "Oklahoma City"
    };

    const char* jobs[] = {
        "Engineer", "Doctor", "Teacher", "Developer", "Designer", "Manager",
        "Analyst", "Scientist", "Writer", "Artist", "Chef", "Driver", "Nurse",
        "Accountant", "Lawyer", "Architect", "Photographer", "Musician", "Athlete"
    };

    srand(time(NULL));
    int first_names_count = sizeof(first_names) / sizeof(first_names[0]);
    int last_names_count = sizeof(last_names) / sizeof(last_names[0]);
    int domains_count = sizeof(domains) / sizeof(domains[0]);
    int cities_count = sizeof(cities) / sizeof(cities[0]);
    int jobs_count = sizeof(jobs) / sizeof(jobs[0]);

    fprintf(file, "# Auto-generated macro with %d records\n", num_records);
    fprintf(file, "# Created at: %s\n", __TIMESTAMP__);
    fprintf(file, "\n");

    for (int i = 1; i <= num_records; i++) {
        const char* first_name = first_names[rand() % first_names_count];
        const char* last_name = last_names[rand() % last_names_count];
        const char* domain = domains[rand() % domains_count];
        const char* city = cities[rand() % cities_count];
        const char* job = jobs[rand() % jobs_count];

        int age = 18 + rand() % 50;
        int salary = 30000 + rand() % 120000;
        int experience = rand() % 20;
        int rating = 1 + rand() % 5;

        // Генерация разнообразного текста
        char email[100];
        snprintf(email, sizeof(email), "%s.%s%d@%s", 
                 first_name, last_name, rand() % 1000, domain);

        char address[200];
        snprintf(address, sizeof(address), "%d %s Street, %s, %s %05d",
                100 + rand() % 9000, last_name, city, "CA", 90000 + rand() % 10000);

        char description[500];
        snprintf(description, sizeof(description),
                "%s %s is a %s with %d years of experience working in %s. "
                "Specializes in advanced technologies and has a rating of %d/5. "
                "Contact at %s or visit at %s",
                first_name, last_name, job, experience, city, rating, email, address);

        char phone[20];
        snprintf(phone, sizeof(phone), "+1-%03d-%03d-%04d",
                200 + rand() % 800, 100 + rand() % 900, 1000 + rand() % 9000);

        // Запись в макрос
        fprintf(file, "INSERT INTO users VALUES (%d, '%s', %d, %d, %d, %d, '%s', '%s', '%s', '%s', %d)\n",
                i,                              // ID
                first_name,                     // Имя
                age,                           // Возраст
                salary,                        // Зарплата
                experience,                    // Опыт
                rating,                        // Рейтинг
                email,                         // Email
                phone,                         // Телефон
                address,                       // Адрес
                description,                   // Описание
                rand() % 2                     // Активный (0 или 1)
        );

        // Добавляем некоторые специальные записи
        if (i % 10 == 0) {
            fprintf(file, "# Special record %d\n", i);
            fprintf(file, "INSERT INTO users VALUES (%d, 'Special_User_%d', %d, %d, %d, %d, "
                    "'special%d@company.com', '+1-800-555-%04d', 'Corporate Office', "
                    "'Premium account with extended features', 1)\n",
                    i + 1000, i, 25 + rand() % 20, 80000 + rand() % 50000,
                    5 + rand() % 15, 5, i, 1000 + i);
        }
    }

    // Добавляем команды для проверки данных
    fprintf(file, "\n# Verification commands\n");
    fprintf(file, "SELECT * FROM users WHERE age > 30\n");
    fprintf(file, "SELECT * FROM users WHERE salary > 50000\n");
    fprintf(file, "FIND TEXT 'Engineer'\n");
    fprintf(file, "FIND TEXT 'special@company.com'\n");
    fprintf(file, "SELECT * FROM users WHERE rating = 5\n");

    fclose(file);
    printf("Macro generated: %s with %d records\n", filename, num_records);
}

int main() {
    printf("Macro Generator for ODQ SQL Console\n");
    printf("===================================\n\n");

    int num_records;
    char filename[100];

    printf("Enter output filename: ");
    scanf("%99s", filename);

    printf("Enter number of records to generate: ");
    scanf("%d", &num_records);

    if (num_records <= 0) {
        printf("Number of records must be positive\n");
        return 1;
    }

    generate_macro(filename, num_records);

    printf("\nUsage:\n");
    printf("1. First create table with appropriate structure:\n");
    printf("   CREATE TABLE users (id int, name text(50), age int, salary int, ");
    printf("experience int, rating int, email text(100), phone text(20), ");
    printf("address text(200), description text(500), active bool)\n\n");
    
    printf("2. Then load and execute macro:\n");
    printf("   USE users\n");
    printf("   LOAD %s\n", filename);

    return 0;
}
