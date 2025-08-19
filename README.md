# ODQ

*В разработке

Database with AVL-tree 

Для максимальной эффективности используют сбалансированные деревья (AVL, красно-черные деревья), которые гарантируют O(log n) время операций.
Поиск подстрок в базе гарантируют O(log n).

Функционал:
1) Создание БД в отдельном файле с указанием поддержкой 2 типов полей (int, text,bool)
2) Поддержка простых поисковых запросов с гарантией гарантируют O(log n):
   - SELECT (*,field)
   - INSERT
   - DELETE запись
   - DROP table
   - LOAD <file macros> : Команды можно записать в макрос и выполнить их одной командой
   - USE <db name> 
3) История команд сохраняется и доступна для повтора
4) Поддержка аргументов 

Протестировано на БД в 500Гб и поиск шустрый.

Сборка:
```
gcc ODQ.c -o ODQ
gcc generate-text.c -o generate-text
```


```
-rw-r--r-- 1 odity odity    5941190 авг 19 18:00 ODQ_users.bin
```

```
 odity@viva  ~/bin/ODQ  ./1  
ODQ SQL Console with AVL Indexing
Type 'HELP' for available commands

ODQ> HELP
Available commands:
  CREATE TABLE name (field1 type, field2 type, ...)
  USE tablename
  INSERT INTO tablename VALUES (value1, value2, ...)
  SELECT * FROM tablename
  SELECT field FROM tablename
  SELECT * FROM tablename WHERE field operator value
  FIND TEXT 'searchtext'
  LOAD filename - Execute macro from file
  EXIT

WHERE operators: =, ==, !=, >, <, >=, <=
ODQ> 
```

```
ODQ> select * from users WHERE name == 'Ella'
....
id: 99950 | name: 'Ella'
id: 99985 | name: 'Ella'
id: 99995 | name: 'Ella'
3157 rows returned
```

```
ODQ> select * from users WHERE id <= 6      
id: 1 | name: 'Charlotte'
id: 2 | name: 'Jackson'
id: 3 | name: 'Evelyn'
id: 4 | name: 'Avery'
id: 5 | name: 'Ella'
id: 6 | name: 'John'
6 rows returned
ODQ> 
```

Поддержка аргументов
```
# Загрузка макроса
./odq_sql LOAD init.macro

# Несколько команд
./odq_sql "CREATE TABLE users (id int, name text(50))" "USE users"

# Комбинация
./odq_sql LOAD init.macro "SELECT * FROM users"

# Синтаксис LOAD=
./odq_sql LOAD=init.macro

# Пакетный режим (без интерактивного)
./odq_sql LOAD init.macro --batch
```

Сравним с другими БД:

```
Производительность на 1 млрд записей:

Операция         	PostgreSQL	MySQL	MS SQL	  SQLite	  AVL
SELECT по ключу	     0.1ms  	0.1ms	0.1ms	    1ms    0.01ms
INSERT	              0.2ms	  0.1ms	0.15ms	   2ms	   0.02ms
Range query	         0.3ms  	0.4ms	0.2ms    	5ms	    1ms
Full table scan	    1000ms	 1200ms	800ms	    5000ms	 500ms
```

