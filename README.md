# Тестовое задание Gaijin

В проекте собирается сервер, два клиента, лоад тест

В некоторых местах я писал логи в std::cerr, т.ч. может быть полезно запускать программы с `2> /dev/null`, если логи не нужны.

## Сервер

Запускается так:

```
./dictionary_server_main <port>
```

В той же директории должен быть файл config.txt

## Клиент cmd

Запускается так:

```
./dictionary_client_cmd <host> <port>
```

Команды подаются, как в постановке задачи, например:
```
$get key
```

```
$set key=value
```

## Load test клиент

Запускается так:
```
./dictionary_load_client <host> <port> <n_requests> <requests_period_us> <keys_list_file> <statistics_output>
```

`key_list_file` -- файл с ключами, которые будут использоваться для запросов. Ключи считываются построчно. Пример есть в `src/load_test/keys.txt`

`statistics_output` -- опционален

1 процент запросов -- `set`, остальное -- `get`.

Чтобы запустить клиент, как требуется в условии, надо выполнить:

```
./dictionary_server_main <port>
```

```
./dictionary_load_client 127.0.0.1 <port> 10000 0 {project_root}/src/load_test/keys.txt
```

## Load test

Запускается так:
```
python3 load_test.py --port PORT --num_requests NUM_REQUESTS --request_period REQUEST_PERIOD --num_clients NUM_CLIENTS --key_file KEY_FILE
```

`dictionary_server_main` и `dictionary_load_client` должны быть в той же директории

Статистика по клиентам будет лежать в `test_res`

Тест не очень масштабируется по клиентам, т.к. всё запускается на одном хосте.
