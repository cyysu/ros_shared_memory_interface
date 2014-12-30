/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, Joshua James
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SHARED_MEMORY_SUBSCRIBER_HPP
#define SHARED_MEMORY_SUBSCRIBER_HPP

#include "shared_memory_transport_impl.hpp"

namespace shared_memory_interface
{
  template<typename T> //T must be the type of a ros message
  class Subscriber
  {
  public:
    Subscriber(bool listen_to_rostopic = true, bool use_polling = false)
    {
      m_listen_to_rostopic = listen_to_rostopic;
      m_use_polling = use_polling;
      m_callback_thread = NULL;
    }

    ~Subscriber()
    {
      if(m_callback_thread != NULL)
      {
        m_callback_thread->interrupt();
        m_callback_thread->detach();
        delete m_callback_thread;
      }
    }

    //just sets up names and subscriber for future getCurrent / waitFor calls
    bool subscribe(std::string topic_name, std::string shared_memory_interface_name = "smi")
    {
      m_interface_name = shared_memory_interface_name;
      configureTopicPaths(m_interface_name, topic_name, m_full_ros_topic_path, m_full_topic_path);
      m_smt.configure(m_interface_name, m_full_topic_path, false);

      bool success = true;
      if(!m_smt.connect(1.0))
      {
        ROS_WARN("Couldn't connect to %s via shared memory! Will try again later! Returning false for now.", m_full_ros_topic_path.c_str());
        success = false;
      }

      if(m_listen_to_rostopic)
      {
        ros::NodeHandle nh("~");
        m_subscriber = nh.subscribe<T>(m_full_ros_topic_path, 1, boost::bind(&Subscriber<T>::blankCallback, this, _1));
      }

      return success;
    }

    bool subscribe(std::string topic_name, boost::function<void(T&)> callback, std::string shared_memory_interface_name = "smi")
    {
      bool success = subscribe(topic_name, shared_memory_interface_name);
      m_callback_thread = new boost::thread(boost::bind(&Subscriber<T>::callbackThreadFunction, this, &m_smt, callback));
      return success;
    }

    bool waitForMessage(T& msg, double timeout = -1)
    {
      if(!m_smt.initialized())
      {
        ROS_DEBUG("Tried to use an uninitialized shared memory transport!");
        return false;
      }
      if(!m_smt.connected() && !m_smt.connect(timeout))
      {
        ROS_DEBUG("Tried to use an unconnected shared memory transport and reconnection attempt failed!");
        return false;
      }
      if(!m_smt.awaitNewDataPolled(msg, timeout))
      {
        return false;
      }

      return true;
    }

    bool getCurrentMessage(T& msg)
    {
      return waitForMessage(msg, 0);
    }

    bool connected()
    {
      return m_smt.connected();
    }

  protected:
    SharedMemoryTransport<T> m_smt;

    std::string m_interface_name;
    std::string m_full_topic_path;
    std::string m_full_ros_topic_path;

    bool m_listen_to_rostopic;
    bool m_use_polling;
    ros::Subscriber m_subscriber;

    boost::thread* m_callback_thread;

    void callbackThreadFunction(SharedMemoryTransport<T>* smt, boost::function<void(T&)> callback)
    {
      T msg;
      std::string serialized_data;

      ros::Rate loop_rate(10.0);
      while(ros::ok()) //wait for the field to at least have something
      {
        if(!smt->initialized())
        {
          ROS_WARN("Shared memory transport was shut down while we were waiting for connections. Stopping callback thread!");
          return;
        }
        if(!smt->connected())
        {
          smt->connect();
        }
        else if(smt->hasData())
        {
          break;
        }
        ROS_WARN_STREAM_THROTTLE(1.0, "Trying to connect to field " << smt->getFieldName() << "...");
        loop_rate.sleep();
        boost::this_thread::interruption_point();
      }

      if(getCurrentMessage(msg))
      {
        callback(msg);
      }

      while(ros::ok())
      {
        try
        {
//          if(!smt->initialized())
//          {
//            ROS_WARN("Shared memory transport was shut down. Stopping callback thread!");
//            return;
//          }
          if(m_use_polling)
          {
            smt->awaitNewDataPolled(msg);
          }
          else
          {
            smt->awaitNewData(msg);
          }
          callback(msg);
        }
        catch(ros::serialization::StreamOverrunException& ex)
        {
          ros::NodeHandle nh("~");
          ROS_ERROR("Deserialization failed for node %s, topic %s! The string was:\n%s", nh.getNamespace().c_str(), m_full_topic_path.c_str(), serialized_data.c_str());
        }
        boost::this_thread::interruption_point();
      }
    }

    void blankCallback(const typename T::ConstPtr& msg)
    {
    }
  };

}
#endif //SHARED_MEMORY_SUBSCRIBER_HPP
