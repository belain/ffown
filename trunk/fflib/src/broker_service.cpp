#include "broker_service.h"
#include "log/log.h"
#include "utility/timer_service.h"
#include "utility/performance_daemon.h"

using namespace ff;

broker_service_t::broker_service_t():
    m_uuid(0),
    m_msg_uuid(100)
{
}

broker_service_t::~broker_service_t()
{
    
}

int broker_service_t::handle_broken(socket_ptr_t sock_)
{
    logwarn((BROKER, "broker_service_t::handle_broken bagin soket_ptr<%p>", sock_));
    lock_guard_t lock(m_mutex);

    vector<uint32_t> del_sgid;
    vector<uint32_t> del_sid;
    vector<uint32_t> del_callback_uuid;

    service_obj_map_t::iterator it = m_service_obj_mgr.begin();
    for (; it != m_service_obj_mgr.end(); ++it)
    {
        map<uint16_t, service_obj_t>::iterator it2 = it->second.service_objs.begin();
        for (; it2 != it->second.service_objs.end(); ++it2)
        {
            callback_map_t::iterator uuid_it = it2->second.m_callback_map.begin();
            for (; uuid_it != it2->second.m_callback_map.end(); ++uuid_it)
            {
                if (uuid_it->second.socket_ptr == sock_)
                {
                    del_callback_uuid.push_back(uuid_it->first);
                }
            }
            
            if (it2->second.socket_ptr == sock_)
            {
                del_sgid.push_back(it->first);
                del_sid.push_back(it2->first);
            }
            
            //! del all tmp callback uuid
            for (size_t i = 0; i < del_callback_uuid.size(); ++i)
            {
                it2->second.m_callback_map.erase(del_callback_uuid[i]);
            }
            del_callback_uuid.clear();
        }
    }
    
    for (size_t i = 0; i < del_sgid.size(); ++i)
    {
        logwarn((BROKER, "broker_service_t::handle_broken del sgid[%u], sid[%u]", del_sgid[i], del_sid[i]));
        m_service_obj_mgr[del_sgid[i]].service_objs.erase(del_sid[i]);
        if (m_service_obj_mgr[del_sgid[i]].service_objs.empty())
        {
            m_service_obj_mgr.erase(del_sgid[i]);
            logwarn((BROKER, "broker_service_t::handle_broken del sgid[%u]", del_sgid[i]));
        }
    }
    
    sock_->safe_delete();
    logwarn((BROKER, "broker_service_t::handle_broken en ok"));
    return 0;
}

int broker_service_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    lock_guard_t lock(m_mutex);

    msg_tool_t msg_tool;
    try{
        msg_tool.decode(msg_.get_body());
    }catch(exception& e_)
    {
        logerror((BROKER, "broker_service_t::handle_msg except<%s>", e_.what()));
        return -1;
    }

    logtrace((BROKER, "broker_service_t::handle_msg begin... cmd[%u], name[%s], sgid[%u], sid[%u], msgid[%u]",
                      msg_.get_cmd(), msg_tool.get_name().c_str(), msg_tool.get_group_id(), msg_tool.get_service_id(), msg_tool.get_msg_id()));
    
    if (msg_tool.get_group_id() == 0 && msg_tool.get_service_id() == 0)
    {
        if (msg_tool.get_msg_id() == rpc_msg_cmd_e::CREATE_SERVICE_GROUP)
        {
                create_service_group_t::in_t in;
                in.decode(msg_.get_body());
                rpc_callcack_t<create_service_group_t::out_t> rcb;
                rcb.init_data(rpc_msg_cmd_e::INTREFACE_CALLBACK, 0, 0, in.get_uuid());
                rcb.set_socket(sock_);
                create_service_group(in, rcb);
        }
        else if (msg_tool.get_msg_id() == rpc_msg_cmd_e::CREATE_SERVICE)
        {
                create_service_t::in_t in;
                in.decode(msg_.get_body());
                rpc_callcack_t<create_service_t::out_t> rcb;
                rcb.init_data(rpc_msg_cmd_e::INTREFACE_CALLBACK, 0, 0, in.get_uuid());
                rcb.set_socket(sock_);
                create_service(in, rcb);
        }
        else if (msg_tool.get_msg_id() == rpc_msg_cmd_e::REG_INTERFACE)
        {
            reg_interface_t::in_t in;
            in.decode(msg_.get_body());
            
            rpc_callcack_t<reg_interface_t::out_t> rcb;
            rcb.init_data(rpc_msg_cmd_e::INTREFACE_CALLBACK, 0, 0, in.get_uuid());
            rcb.set_socket(sock_);

            reg_interface(in, rcb);
        }
        else if (msg_tool.get_msg_id() == rpc_msg_cmd_e::SYNC_ALL_SERVICE)
        {
            loginfo((BROKER, "broker_service_t::handle_msg begin... cmd[%u], name[%s], sgid[%u], sid[%u], msgid[%u] sock[%p]",
                      msg_.get_cmd(), msg_tool.get_name().c_str(), msg_tool.get_group_id(), msg_tool.get_service_id(), msg_tool.get_msg_id(), sock_));

            sync_all_service_t::in_t in;
            in.decode(msg_.get_body());
            
            rpc_callcack_t<sync_all_service_t::out_t> rcb;
            rcb.init_data(rpc_msg_cmd_e::INTREFACE_CALLBACK, 0, 0, in.get_uuid());
            rcb.set_socket(sock_);
            
            sync_all_service(in, rcb);
        }
    }
    else
    {
        service_obj_map_t::iterator obj_mgr_it = m_service_obj_mgr.find(msg_tool.get_group_id());
        if (obj_mgr_it == m_service_obj_mgr.end())
        {
            logerror((BROKER, "broker_service_t::handle_msg sgid not found cmd[%u], name[%s], sgid[%u], sid[%u], msgid[%u]",
                              msg_.get_cmd(), msg_tool.get_name().c_str(), msg_tool.get_group_id(), msg_tool.get_service_id(), msg_tool.get_msg_id()));
            return -1;
        }

        map<uint16_t, service_obj_t>::iterator sobj_it = obj_mgr_it->second.service_objs.find(msg_tool.get_service_id());
        if (sobj_it == obj_mgr_it->second.service_objs.end())
        {
            logerror((BROKER, "broker_service_t::handle_msg sid not found cmd[%u], name[%s], sgid[%u], sid[%u], msgid[%u]",
                      msg_.get_cmd(), msg_tool.get_name().c_str(), msg_tool.get_group_id(), msg_tool.get_service_id(), msg_tool.get_msg_id()));
            return -1;
        }

        switch (msg_.get_cmd())
        {
            case rpc_msg_cmd_e::CALL_INTERFACE:
            {
                sobj_it->second.async_call(msg_tool, msg_.get_body(), sock_);
            }break;
            case rpc_msg_cmd_e::INTREFACE_CALLBACK:
            {
                sobj_it->second.interface_callback(msg_tool, msg_.get_body());
            }break;
        }
    }

    return 0;
}

void broker_service_t::sync_all_service(sync_all_service_t::in_t& in_msg_, rpc_callcack_t<sync_all_service_t::out_t>& cb_)
{
    sync_all_service_t::out_t ret;

    service_obj_map_t::iterator it = m_service_obj_mgr.begin();
    
    for (; it != m_service_obj_mgr.end(); ++it)
    {
        ret.group_name_vt.push_back(it->second.name);
        ret.group_id_vt.push_back(it->second.id);
        
        map<uint16_t, service_obj_t>::iterator it2 = it->second.service_objs.begin();
        for (; it2 != it->second.service_objs.end(); ++it2)
        {
            sync_all_service_t::id_info_t id_info;
            id_info.sgid = it2->second.group_id;
            id_info.sid  = it2->second.id;

            ret.id_info_vt.push_back(id_info);
        }
    }
    
    map<string, uint16_t>& all_msg = singleton_t<msg_name_store_t>::instance().all_msg();
    for (map<string, uint16_t>::iterator it3 = all_msg.begin(); it3 != all_msg.end(); ++it3)
    {
        ret.msg_name_vt.push_back(it3->first);
        ret.msg_id_vt.push_back(it3->second);
    }
    
    ret.set_uuid(in_msg_.get_uuid());
    cb_(ret);
}

void broker_service_t::create_service_group(create_service_group_t::in_t& in_msg_, rpc_callcack_t<create_service_group_t::out_t>& cb_)
{
    logtrace((BROKER, "broker_service_t::create_service_group begin... service_name<%s>", in_msg_.service_name.c_str()));
 
    create_service_group_t::out_t ret;
    service_obj_map_t::iterator it = m_service_obj_mgr.begin();
    
    for (; it != m_service_obj_mgr.end(); ++it)
    {
        if (it->second.name == in_msg_.service_name)
        {
            break;
        }
    }

    if (it != m_service_obj_mgr.end())
    {
        ret.service_id = it->second.id;
        loginfo((BROKER, "broker_service_t::create_service_group begin... service_name<%s> has exist", in_msg_.service_name.c_str()));
    }
    else
    {
        service_obj_mgr_t obj_mgr;
        obj_mgr.id = ++m_uuid;
        obj_mgr.name = in_msg_.service_name;
        //obj_mgr.socket_ptr = cb_.get_socket();
        ret.service_id = obj_mgr.id;
        m_service_obj_mgr.insert(make_pair(obj_mgr.id, obj_mgr));
    }

    ret.set_uuid(in_msg_.get_uuid());
    cb_(ret);
}

void broker_service_t::create_service(create_service_t::in_t& in_msg_, rpc_callcack_t<create_service_t::out_t>& cb_)
{
    logtrace((BROKER, "broker_service_t::create_service begin... sgid<%u>, sid[%u]", in_msg_.new_service_group_id, in_msg_.new_service_id));

    create_service_t::out_t ret;
    service_obj_mgr_t& som = m_service_obj_mgr[in_msg_.new_service_group_id];
    map<uint16_t, service_obj_t>::iterator it = som.service_objs.find(in_msg_.new_service_id);

    if (it != som.service_objs.end())
    {
        ret.value = false;

        logerror((BROKER, "broker_service_t::create_service failed... sgid<%u>, sid[%u] exist", in_msg_.new_service_group_id, in_msg_.new_service_id));
    }
    else
    {
        service_obj_t obj;
        obj.name = som.name;
        obj.group_id = in_msg_.new_service_group_id;
        obj.id       = in_msg_.new_service_id;
        obj.socket_ptr = cb_.get_socket();

        som.service_objs[in_msg_.new_service_id] = obj;
        
        ret.value = true;
    }
    ret.set_uuid(in_msg_.get_uuid());
    cb_(ret);
}

void broker_service_t::reg_interface(reg_interface_t::in_t& in_msg_, rpc_callcack_t<reg_interface_t::out_t>& cb_)
{
    reg_interface_t::out_t ret;
    ret.alloc_id = -1;
    ret.set_uuid(in_msg_.get_uuid());

    logtrace((BROKER, "broker_service_t::reg_interface sgid[%u], sid[%u]", in_msg_.sgid, in_msg_.sid));

    service_obj_mgr_t& som = m_service_obj_mgr[in_msg_.sgid];
    map<uint16_t, service_obj_t>::iterator it = som.service_objs.find(in_msg_.sid);
    
    if (it != som.service_objs.end())
    {
        uint16_t tmp_id = singleton_t<msg_name_store_t>::instance().name_to_id(in_msg_.in_msg_name);
        if (tmp_id != 0)
        {
            ret.alloc_id = tmp_id;
        }
        else
        {
            ret.alloc_id = ++ m_msg_uuid;
        }
        tmp_id = singleton_t<msg_name_store_t>::instance().name_to_id(in_msg_.out_msg_name);
        if (tmp_id != 0)
        {
            ret.out_alloc_id = tmp_id;
        }
        else
        {
            ret.out_alloc_id = ++ m_msg_uuid;
        }
        
        singleton_t<msg_name_store_t>::instance().add_msg(in_msg_.in_msg_name, ret.alloc_id);
        singleton_t<msg_name_store_t>::instance().add_msg(in_msg_.out_msg_name, ret.out_alloc_id);

        logtrace((BROKER, "broker_service_t::reg_interface sgid[%u], sid[%u] alloc_id[%u]", in_msg_.sgid, in_msg_.sid, ret.alloc_id));
    }

    cb_(ret);
}

void broker_service_t::service_obj_t::async_call(msg_i& msg_, const string& body_, socket_ptr_t sp_)
{
    proc_stack_t stack;
    struct timeval now;
    gettimeofday(&now, NULL);
    
    stack.start_time = now.tv_sec*1000000 + now.tv_usec;

    stack.uuid    = msg_.get_uuid();
    stack.req_msg = msg_.get_name(); 
    stack.socket_ptr= sp_;

    uint32_t uuid = ++m_uuid;

    //! 直接修改消息包内的uuid
    string dest = body_;
    *((uint32_t*)dest.data()) = uuid;

    m_callback_map[uuid] = stack;
    msg_sender_t::send(socket_ptr, rpc_msg_cmd_e::CALL_INTERFACE , dest);
    logtrace((BROKER, "broker_service_t::service_obj_t::async_call socket<%p>", socket_ptr));
    
}

int broker_service_t::service_obj_t::interface_callback(msg_i& msg_, const string& body_)
{
    callback_map_t::iterator it = m_callback_map.find(msg_.get_uuid());
    if (it != m_callback_map.end())
    {
        string dest = body_;
        *((uint32_t*)dest.data()) = it->second.uuid;

        msg_sender_t::send(it->second.socket_ptr, rpc_msg_cmd_e::INTREFACE_CALLBACK, dest);
        
        struct timeval now;
        gettimeofday(&now, NULL);

        long cost = now.tv_sec*1000000 + now.tv_usec - it->second.start_time;

        singleton_t<performance_daemon_t>::instance().post(it->second.req_msg, cost);
        m_callback_map.erase(it);
        return 0;
    }
    else
    {
        logerror((BROKER, "broker_service_t::service_obj_t::interface_callback none uuid[%u]", msg_.get_uuid()));
    }

    return -1;
}
