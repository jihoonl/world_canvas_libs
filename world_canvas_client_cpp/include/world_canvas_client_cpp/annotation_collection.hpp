/*
 * annotation_collection.hpp
 *
 *  Created on: May 7, 2014
 *      Author: jorge
 */

#ifndef ANNOTATION_COLLECTION_HPP_
#define ANNOTATION_COLLECTION_HPP_

#include <ros/serialization.h>
#include <ros/serialized_message.h>

#include <world_canvas_msgs/Annotation.h>
#include <world_canvas_msgs/AnnotationData.h>

#include "world_canvas_client_cpp/filter_criteria.hpp"


/**
 * Manages a collection of annotations and its associated data, initially empty.
 * Annotations and data are retrieved from the world canvas server, filtered by
 * the described parameters.
 * This class can also publish the retrieved annotations and RViz visualization
 * markers, mostly for debug purposes.
 */
class AnnotationCollection
{
private:
  ros::NodeHandle nh;
  ros::Publisher markers_pub;

  FilterCriteria filter;

  std::vector<world_canvas_msgs::Annotation>     annotations;
  std::vector<world_canvas_msgs::AnnotationData> annots_data;

  /**
   * Return data IDs for the current annotations collection. This method is private
   * because is only necessary for loading annotations data from server, operation
   * that must be transparent for the user.
   *
   * @returns Vector of unique IDs.
   */
  std::vector<UniqueIDmsg> getAnnotsDataIDs();

public:
  /**
   * Initializes the collection of annotations and associated data, initially empty.
   *
   * @param world: Annotations in this collection belong to this world.
   */
  AnnotationCollection(const std::string& world);

  /**
   * Initializes the collection of annotations and associated data, initially empty.
   *
   * @param criteria: Annotations filter criteria to pass to the server (must contain
   * at least a valid world name).
   */
  AnnotationCollection(const FilterCriteria& criteria);

  virtual ~AnnotationCollection();

  /**
   * Reload annotations collection, filtered by new selection criteria.
   *
   * @param criteria: Annotations filter criteria to pass to the server.
   * @returns True on success, False otherwise.
   */
  bool filterBy(const FilterCriteria& criteria);

  /**
   * Load associated data for the current annotations collection.
   *
   * @returns True on success, False otherwise.
   */
  bool loadData();

  /**
   * Publish RViz visualization markers for the current collection of annotations.
   *
   * @param topic: Where we must publish annotations markers.
   * @returns True on success, False otherwise.
   */
  bool publishMarkers(const std::string& topic);

  /**
   * Publish the current collection of annotations, by this client or by the server.
   * As we use just one topic, all annotations must be of the same type (function will
   * return with error otherwise).
   *
   * @param topic_name: Where we must publish annotations data.
   * @param by_server:  Request the server to publish the annotations instead of this client.
   * @param as_list:    If true, annotations will be packed in a list before publishing,
   *                    so topic_type must be an array of currently loaded annotations.
   * @param topic_type: The message type to publish annotations data.
   *                    Mandatory if as_list is true; ignored otherwise.
   * @returns True on success, False otherwise.
   */
  bool publish(const std::string& topic_name, bool by_server = false, bool as_list = false,
               const std::string& topic_type = "");

  /**
   * Return unique IDs for the current annotations collection.
   *
   * @returns Vector of unique IDs.
   */
  std::vector<UniqueIDmsg> getAnnotationIDs();

  /**
   * Return the current annotations collection.
   *
   * @returns Vector of annotations.
   */
  const std::vector<world_canvas_msgs::Annotation>& getAnnotations() { return annotations; }

  /**
   * Return the data for annotations of the template type.
   *
   * @param data Vector of annotations data.
   */
  template <typename T>
  unsigned int getData(std::vector<T>& data)
  {
    unsigned int count = 0;
    std::string type = ros::message_traits::DataType<T>::value();

    for (unsigned int i = 0; i < annots_data.size(); ++i)
    {
      // Check id this data corresponds to an annotation of the requested type
      // Would be great to avoid this param, as is implicit in the template type, but I found no way
      // to A) get type string from the type B) make deserialize discriminate messages of other types
      // TODO my god!!! do this smarter!!! we should already contain anns + data as a list of tuples!
      bool right_type = false;

      for (unsigned int j = 0; j < annotations.size(); ++j)
      {
        if ((annotations[j].data_id.uuid == annots_data[i].id.uuid) && (annotations[j].type == type))
        {
          right_type = true;
          break;
        }
      }

      if (! right_type)
        continue;

      // Deserialize the ROS message contained on data field; we must clone the serialized data
      // because SerializedMessage requires a boost::shared_array as input that will try to destroy
      // the underlying array once out of scope. Also we must skip 4 bytes on message_start pointer
      // because the first 4 bytes on the buffer contains the (already known) serialized data size.
      T object;
      uint32_t serial_size = annots_data[i].data.size();
      boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);
      memcpy(buffer.get(), &annots_data[i].data[0], serial_size);
      ros::SerializedMessage sm(buffer, serial_size);
      sm.type_info = &typeid(object);
      sm.message_start += 4;
      try
      {
        ROS_DEBUG("Deserialize object %x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x of type %s",
                 annots_data[i].id.uuid[0], annots_data[i].id.uuid[1], annots_data[i].id.uuid[2], annots_data[i].id.uuid[3],
                 annots_data[i].id.uuid[4], annots_data[i].id.uuid[5], annots_data[i].id.uuid[6], annots_data[i].id.uuid[7],
                 annots_data[i].id.uuid[8], annots_data[i].id.uuid[9], annots_data[i].id.uuid[10], annots_data[i].id.uuid[11],
                 annots_data[i].id.uuid[12], annots_data[i].id.uuid[13], annots_data[i].id.uuid[14], annots_data[i].id.uuid[15],
                 type.c_str());
        ros::serialization::deserializeMessage(sm, object);
        data.push_back(object);
        count++;
      }
      catch (ros::serialization::StreamOverrunException& e)
      {
        ROS_ERROR("Deserialization failed on object %d: %s", i, e.what());
      }
    }
    return count;
  }
};

#endif /* ANNOTATION_COLLECTION_HPP_ */
