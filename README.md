# udp_server_client_filetransfer
Сервис передачи файлов по протоколу UDP с проверкой целостности переданных файлов. 

Сборка проекта:
```
$ cmake ./
$ cmake --build ./
```
Запуск проекта:
```
$cmake --build ./ --target run
```
При запуске, демонстрационная програма сама создаст два файла: 1024Кб и 1Кб со случайным содержимым для передачи серверу, запустит сервер и клиент и начнет передачу созданных фалов.<br />
По окончании передачи в консоли появятся сообщения ```file1.tx send file OK```, это говори о том что передача файла прошла успешно и контрольная сумма, вычисленная на строне сервера, совпадает с контролькой суммой вычисленной на стороне клиента.

###### Примечание:<br />
Иногда могут наблюдаться такие артефакты:
```
file1.tx: Receive chunk id 417
file1.tx: Send ACK chunk id 417
file1.tx: Receive chunk id 14
file1.tx: send file OK                << передача файла была успешна

file1.tx: Receive ACK chunk id 197    << какие-то сообщения о передаче сегментов
file1.tx: Send ACK chunk id 14        << относящихся к этому файлу
```
Это происходит из-за многопоточности и несвоевременном сбросе буфера стандартного вывода.

### valgrind report:
```
<?xml version="1.0"?>

<valgrindoutput>

<protocolversion>4</protocolversion>
<protocoltool>memcheck</protocoltool>

<preamble>
  <line>Memcheck, a memory error detector</line>
  <line>Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.</line>
  <line>Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info</line>
  <line>Command: ./udp_server_client_filetransfer</line>
</preamble>

<pid>197549</pid>
<ppid>131570</ppid>
<tool>memcheck</tool>

<args>
  <vargv>
    <exe>/usr/bin/valgrind.bin</exe>
    <arg>--leak-check=full</arg>
    <arg>--track-origins=yes</arg>
    <arg>--xml=yes</arg>
    <arg>--xml-file=./ValgrindOut.xml</arg>
  </vargv>
  <argv>
    <exe>./udp_server_client_filetransfer</exe>
  </argv>
</args>

<status>
  <state>RUNNING</state>
  <time>00:00:00:00.218 </time>
</status>


<status>
  <state>FINISHED</state>
  <time>00:00:00:24.652 </time>
</status>

<errorcounts>
</errorcounts>

<suppcounts>
</suppcounts>

</valgrindoutput>
```
