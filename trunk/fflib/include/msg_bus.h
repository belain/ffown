//! 消息发送管理
#ifndef _MSG_BUS_H_
#define _MSG_BUS_H_

class rpc_service_t;
class rpc_service_group_t;

#include <string>
#include <map>
using namespace std;

#include "net_factory.h"
#include "rpc_service.h"
#include "rpc_service_group.h"
#include "rpc_future.h"

class msg_bus_t: public msg_handler_i
{
    typedef map<uint16_t, rpc_service_group_t*>  service_map_t;
public:
    msg_bus_t();
    ~msg_bus_t();

    rpc_service_group_t& create_service_group(const string& name_);
    rpc_service_t&       create_service(const string& name_, uint16_t id_);

    rpc_service_group_t* get_service_group(uint16_t id_);

    int handle_broken(socket_ptr_t sock_);
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    int open(const string& host_);
    socket_ptr_t get_socket(const rpc_service_t* rs_);

    int register_service(const string& name_, uint16_t id_);
    
private:
    uint32_t            m_uuid;
    service_map_t       m_service_map;
    acceptor_i*         m_acceptor;
    rpc_service_t*      m_broker_service;
    socket_ptr_t        m_socket;
};

 #endif
