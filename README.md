A shared-memory-based transport library that exposes a publisher/subscriber interface similar to the one used by ROS.

To run basic test:
    $ roscore
    $ rosrun shared_memory_interface shared_memory_manager 
    $ rosrun shared_memory_interface_tutorials tutorial_ros_talker
    $ rosrun shared_memory_interface_tutorials tutorial_ros_listener