#include <iostream>
#include <signal.h>
#include "protocol.h"
#include "server.h"
#include "client.h"
#include "udp_server.h"
#include "udp_client.h"
#include <thread>
#include <iterator>
#include "observer_base.h"

void run_server();
void run_client(std::string filename);
void create_sized_file(std::string filename,size_t size);

observer_base observer;

using namespace std;


static volatile bool keepRunning = true;
void termination_handler(int)
{
    keepRunning = false;
}

int main() {

    signal(SIGTERM, termination_handler);
    signal(SIGSTOP, termination_handler);
    signal(SIGINT,  termination_handler);
    signal(SIGQUIT, termination_handler);

    //отправлять будем два файла - один маленький
    //поместится в один UDP пакет, второй большой
    create_sized_file("file1.txt", 1*1024*1024);
    create_sized_file("file2.txt", 1024);

    //Создаем отдельные потоки для сервера и клиента.
    std::thread t1(run_server);
    std::thread t2(run_client,"file1.txt");
    std::thread t3(run_client,"file2.txt");

    //ждем завершения клиентских потоков
    t2.join();
    t3.join();

	keepRunning = false;

	t1.join();

	return 0;
}

/// функция потока для сервера
void run_server() {
    udp_server udp_srv;//UDP server
    server_context srv_context(&udp_srv);//логика получения файлов

    srv_context.register_observer(&observer);//регистрируем обсервер для получения событий из srv_context

    //были мысли сделать имитацию потери пакетов - но тогда проект превысит лимит в 10 файлов...

    udp_srv.init(50);
    auto a = udp_srv.add_ss(2000); //сервер будет работать на 2000 порту
    if(*a > 0) {
        //регистрируем функцию обратного вызова для всех пакетов приходящих на этот
        //сокет, чтобы отделить бизнеслогику от транспорта
        a->set_onRead(std::bind(&server_context::receive_packet, &srv_context, std::placeholders::_1,std::placeholders::_2,
                                std::placeholders::_3, std::placeholders::_4));
    }

    //основной цикл получения пакетов
	while(keepRunning) {
        udp_srv.do_ss();
    }
}

void create_sized_file(std::string filename,size_t size) {
    std::ofstream file(filename, ios::trunc | ios::binary);
    std::mt19937 gen{ std::random_device()() };
    std::uniform_int_distribution<> dis(0, 255);
    std::generate_n(std::ostream_iterator<char>(file, ""), size, [&]{ return dis(gen); });

}

/// функция потока для клиента
void run_client(std::string filename) {
    std::ifstream file(filename, std::ifstream::binary);
    if(file.is_open()){
        //получаем имя файла и на его основе делаем file ID,
        //можно конечно было использовать идентификатор file,
        //но тогда это бы выглядело не красиво в логах.
        std::string file_id = std::filesystem::path(filename ).filename().string();
        file_id.resize(8);
        BYTE id[8];
        std::memcpy(&id[0],file_id.data(),8);

        //создаем UDP клиент и регистрируем функции обратного вызова.
        udp_client_callbacks cb;
        udp_client udp_clnt("127.0.0.1", 2000, SOCK_DGRAM);
        client_context clnt_context(&udp_clnt);
        clnt_context.register_observer(&observer);
        cb.on_read = std::bind(&client_context::receive_packet, &clnt_context, std::placeholders::_1,std::placeholders::_2);
        udp_clnt.set_cb(cb);

        volatile bool th_running = true;

        //Задача маленькая, можем позволить себе такое ))
        std::thread tn([&udp_clnt,&th_running]()
           {
                while(th_running)
                {
                    udp_clnt.do_cs();
                }
           });

        //отправляем файл пока не достигнем успеха
        while(keepRunning) {
            if(clnt_context.send_file(id, file)){
                sleep(1);
                std::cout << std::flush << file_id << ": send file OK\n" << std::endl << std::flush;
                break;
            }
            else
            {
                sleep(1);
                std::cerr << std::flush << file_id << ": send file fail\nretry\n" << std::endl << std::flush;
            }
        }

        th_running = false;

        tn.join();
        file.close();
    }
    else {
        std::cerr<<"file not found\n"<<std::endl;
    }
}
