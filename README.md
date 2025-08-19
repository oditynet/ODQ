# ODQ
Database with AVL-tree 

Для максимальной эффективности используют сбалансированные деревья (AVL, красно-черные деревья), которые гарантируют O(log n) время операций.
Поиск подстрок в базе гарантируют O(log n).

Функционал:
1) Создание БД в отдельном файле с указанием поддержкой 2 типов полей (int, text)
2) Поддержка простых поисковых запросов с гарантией гарантируют O(log n)
3) Поддержка макросов (генерируеются отдельным файлом) LOAD <filename>
4) История команд сохраняется и доступна для повтора

Протестировано на БД в 500Гб и поиск шустрый.

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

