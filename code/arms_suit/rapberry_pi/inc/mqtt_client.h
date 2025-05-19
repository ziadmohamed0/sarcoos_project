#ifndef _INC_MQTT_CLIENT_H_
#define _INC_MQTT_CLIENT_H_

#include <mqtt/async_client.h>
#include <string>
#include <functional>

class mqtt_client {
	public:
		mqtt_client(const std::string& server, const std::string& client_id);
		void connect();
		void subscribe(const std::string& topic);
		void publisher(const std::string& topic, const std::string& payload);
		void set_callback(std::function<void(std::string&)> msg);
		std::string get_payload();
	private:
		mqtt::async_client client;
		std::string last_payload;
		class Callback : public virtual mqtt::callback {
			public:
				Callback(mqtt_client& parent, std::function<void(std::string&)> usr_cb);
				void message_arrived(mqtt::const_message_ptr msg) override;
			private:
				mqtt_client& parent_ref;
				std::function<void(std::string&)> user_callback;
		};
		std::unique_ptr<Callback> callback;
};

#endif
