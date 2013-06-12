/*
 * bag_file_recorder.h
 *
 *  Created on: Jun 7, 2013
 *      Author: pastor
 */

#ifndef BAG_FILE_RECORDER_H_
#define BAG_FILE_RECORDER_H_

#include <ros/ros.h>
#include <rosbag/bag.h>
#include <usc_utilities/assert.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include <task_recorder2_msgs/Description.h>
#include <task_recorder2/task_recorder_io.h>
#include <task_recorder2/msg_header_utility.h>

#include <task_recorder2_utilities/task_description_utilities.h>
#include <task_recorder2_utilities/task_recorder_utilities.h>

namespace task_recorder2
{

static const std::string BAG_DIRECTORY_NAME = "bags";

class BagFileRecorderBase
{
public:
  BagFileRecorderBase() {};
  virtual ~BagFileRecorderBase() {};

  /*!
   * @param description
   * @param id
   * @return True on success, otherwise False
   */
  virtual bool startRecording(const std::string& description, const int id) = 0;

  /*!
   * @param crop_start_time
   * @param crop_end_time
   * @return True on success, otherwise False
   */
  virtual bool stopRecording(const ros::Time& crop_start_time, const ros::Time& crop_end_time) = 0;

  /*!
   * @return True if recording, otherwise False
   */
  virtual bool isRecording() = 0;

private:

};

template<class MessageType, class MessageTypeStamped>
  class BagFileRecorder : public BagFileRecorderBase
  {

  public:

  typedef boost::shared_ptr<MessageType const> MessageTypeConstPtr;

  BagFileRecorder();
  virtual ~BagFileRecorder() {};

  bool initialize(ros::NodeHandle node_handle, const std::string& topic_name,
                  const boost::shared_ptr<task_recorder2::HeaderUtility<MessageType> > header_utility);

  /*!
   * @param description
   * @param id
   * @return True on success, otherwise False
   */
  bool startRecording(const std::string& description, const int id);

  /*!
   * @param crop_start_time
   * @param crop_end_time
   * @return True on success, otherwise False
   */
  bool stopRecording(const ros::Time& crop_start_time, const ros::Time& crop_end_time);

  /*!
   * @return True if recording, otherwise False
   */
  bool isRecording();

  private:

    /*!
     */
    TaskRecorderIO<MessageTypeStamped> recorder_io_;

    /*!
     */
    static const int MESSAGE_SUBSCRIBER_BUFFER_SIZE = 10000;
    ros::Subscriber message_subscriber_;

    void recordMessagesCallback(const MessageTypeConstPtr message);

    boost::mutex mutex_;
    bool is_recording_;
    ros::Time abs_start_time_;

    void setRecording(const bool recording);
    void waitForMessages();

    boost::shared_ptr<HeaderUtility<MessageType> > header_utility_;
  };

template<class MessageType, class MessageTypeStamped>
  BagFileRecorder<MessageType, MessageTypeStamped>::BagFileRecorder()
 : recorder_io_(ros::NodeHandle("/TaskRecorderManager")), is_recording_(false)
{
}

template<class MessageType, class MessageTypeStamped>
  bool BagFileRecorder<MessageType, MessageTypeStamped>::initialize(ros::NodeHandle node_handle, const std::string& topic_name,
                                                const boost::shared_ptr<task_recorder2::HeaderUtility<MessageType> > header_utility)
{
  ROS_VERIFY(recorder_io_.initialize(topic_name, ""));
  message_subscriber_ = recorder_io_.node_handle_.subscribe(recorder_io_.topic_name_,
                                                            MESSAGE_SUBSCRIBER_BUFFER_SIZE,
                                                            &BagFileRecorder<MessageType, MessageTypeStamped>::recordMessagesCallback, this);
  header_utility_ = header_utility;
  return true;
}


template<class MessageType, class MessageTypeStamped>
  bool BagFileRecorder<MessageType, MessageTypeStamped>::startRecording(const std::string& description, const int id)
  {
    task_recorder2_msgs::Description description_msg;
    description_msg.description = description;
    description_msg.id = id;
    // recorder_io_.setDescription(description_msg);
    recorder_io_.setDescription(description_msg, BAG_DIRECTORY_NAME);

    mutex_.lock();
    is_recording_ = true;
    recorder_io_.messages_.clear();
    mutex_.unlock();

    ROS_INFO("Waiting for topic named >%s< with description >%s< to id >%i<.", recorder_io_.topic_name_.c_str(), description.c_str(), id);
    waitForMessages();
    ROS_INFO("Recording topic named >%s< with description >%s< to id >%i<.", recorder_io_.topic_name_.c_str(), description.c_str(), id);

    return true;
  }

template<class MessageType, class MessageTypeStamped>
  void BagFileRecorder<MessageType, MessageTypeStamped>::waitForMessages()
  {
    // wait for 1st message.
    bool no_message = true;
    ROS_DEBUG("Waiting for message >%s<", recorder_io_.topic_name_.c_str());
    while (ros::ok() && no_message)
    {
      ros::spinOnce();
      mutex_.lock();
      no_message = recorder_io_.messages_.empty();
      if (!no_message)
      {
        abs_start_time_ = recorder_io_.messages_[0].header.stamp;
      }
      mutex_.unlock();
      ros::Duration(0.01).sleep();
    }
  }

template<class MessageType, class MessageTypeStamped>
  bool BagFileRecorder<MessageType, MessageTypeStamped>::stopRecording(const ros::Time& crop_start_time, const ros::Time& crop_end_time)
  {
    ROS_INFO("Stop recording...");
    if (!isRecording())
    {
      ROS_ERROR("Bag file on topic >%s< is not being recorder, cannot stop recording.", recorder_io_.topic_name_.c_str());
      return false;
    }

    boost::mutex::scoped_lock lock(mutex_);
    if (false) // no cropping for now...
    {

      if (crop_start_time < abs_start_time_)
      {
        ROS_ERROR("Bag file was recorded starting from >%f<, cannot crop starting from >%f<",
                  abs_start_time_.toSec(), crop_start_time.toSec());
        return false;
      }

      if (recorder_io_.messages_.back().header.stamp < crop_end_time)
      {
        ROS_ERROR("Bag file was recorded starting from >%f<, cannot crop starting from >%f<",
                  abs_start_time_.toSec(), crop_start_time.toSec());
        return false;
      }

      // stop recording
      is_recording_ = false;

      bool found = false;
      unsigned int start_index = 0;
      for (unsigned int i = 0; !found && i < recorder_io_.messages_.size(); ++i)
      {
        if (recorder_io_.messages_[i].header.stamp > crop_start_time)
        {
          start_index = i;
          found = true;
        }
      }
      found = false;
      int end_index = 0;
      for (int i = (int)recorder_io_.messages_.size() - 1; !found && i >= 0; --i)
      {
        if (recorder_io_.messages_[i].header.stamp < crop_end_time)
        {
          end_index = i;
          found = true;
        }
      }

      // crop
      recorder_io_.messages_.erase(recorder_io_.messages_.end() - ((int)recorder_io_.messages_.size() - end_index),
                                   recorder_io_.messages_.end());
      recorder_io_.messages_.erase(recorder_io_.messages_.begin(), recorder_io_.messages_.begin() + start_index);
    }

    // write
    if(!recorder_io_.writeRecordedData(BAG_DIRECTORY_NAME, true))
    {
      return false;
    }
    // boost::thread(boost::bind(&TaskRecorderIO<MessageTypeStamped>::writeRawData, recorder_io_));

    return true;
  }

template<class MessageType, class MessageTypeStamped>
  void BagFileRecorder<MessageType, MessageTypeStamped>::setRecording(const bool recording)
  {
    boost::mutex::scoped_lock lock(mutex_);
    ROS_DEBUG_COND(is_recording_ && !recording, "Stop recording topic named >%s<.", recorder_io_.topic_name_.c_str());
    ROS_DEBUG_COND(!is_recording_ && recording, "Start recording topic named >%s<.", recorder_io_.topic_name_.c_str());
    is_recording_ = recording;
  }

template<class MessageType, class MessageTypeStamped>
  bool BagFileRecorder<MessageType, MessageTypeStamped>::isRecording()
  {
    bool is_recording;
    boost::mutex::scoped_lock lock(mutex_);
    is_recording = is_recording_;
    return is_recording;
  }

template<class MessageType, class MessageTypeStamped>
  void BagFileRecorder<MessageType, MessageTypeStamped>::recordMessagesCallback(const MessageTypeConstPtr message)
  {
    // ROS_INFO("Callback for topic >%s<.", recorder_io_.topic_name_.c_str());
    boost::mutex::scoped_lock lock(mutex_);
    if (is_recording_)
    {
      // double delay = (ros::Time::now() - data_sample_.header.stamp).toSec();
      // ROS_INFO("Delay = %f", delay);
//      ROS_INFO("Logging >%s<.", recorder_io_.topic_name_.c_str());
//      if(header_utility_)
//      {
        MessageTypeStamped msg;
        msg.msg = *message;
        msg.header = header_utility_->header(*message);
        recorder_io_.messages_.push_back(msg);
//
//      }

      // recorder_io_.messages_.push_back(*message);
    }
  }
}


#endif /* BAG_FILE_RECORDER_H_ */