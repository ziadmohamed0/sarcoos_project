#include "../inc/mqtt_client.h"
#include <chrono>

mqtt_client::mqtt_client(const std::string& server, const std::string& client_id) : 
		client(server, client_id),
		last_payload(nullptr){}

void mqtt_client::connect() {
	this->client.connect()->wait();
}

void mqtt_client::subscribe(const std::string& topic) {
	this->client.subscribe(topic, 1)->wait();
}

void mqtt_client::publisher(const std::string& topic, const std::string& payload) {
	auto msg = mqtt::make_message(topic, payload);
	msg->set_qos(1);
    client.publish(msg)->wait_for(std::chrono::seconds(10));
}

void mqtt_client::set_callback(std::function<void(std::string&)> msg) {
	callback = std::make_unique<Callback>(*this, msg);
	client.set_callback(*callback);
}

std::string mqtt_client::get_payload() {
	return this->last_payload;
}

mqtt_client::Callback::Callback(mqtt_client& parent, std::function<void(std::string&)> usr_cb) :
				parent_ref(parent),
				user_callback(usr_cb){

}

void mqtt_client::Callback::message_arrived(mqtt::const_message_ptr msg) {
	this->parent_ref.last_payload = msg->to_string();
	if(this->user_callback) user_callback(this->parent_ref.last_payload);
}
