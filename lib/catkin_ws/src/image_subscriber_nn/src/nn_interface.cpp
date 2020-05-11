
#include "../include/image_subscriber_nn/nn_interface.h"
#define SPEED 0.2
//namespace enc = sensor_msgs::image_encodings;
int idx = 0;
bool flag = false;
//std::mutex sensor_mtx;
//std::condition_variable cond_v;
int sensor_weights[4] = {4,3,2,1};
NeuralNetworkInterface *NeuralNetworkInterface::interface=0;

float sensor_values[8];

void NeuralNetworkInterface::sensor_callback(const sensor_msgs::Image::ConstPtr& msg, int index){
	sensor_values[index] = (1-msg->data[0]/255.0);
	//TODO have a better way to check for recpetion of all sensor values...
	idx++;
	if(idx==8){
		idx=0;
		flag = true;
		cout << "Sensor values: "<<sensor_values[0] <<" " <<sensor_values[1] <<" "<<sensor_values[2] <<" "<<sensor_values[3] <<" "<<sensor_values[4] <<" "<<sensor_values[5] <<" "<<sensor_values[6] <<" "<<sensor_values[7] << endl;
	}else{
		flag = false;
	}
}


float NeuralNetworkInterface::calc_error(){
	//while (!flag){}; //TODO: Change from busy waiting
	float error = 0;
	sensor_values[4] - sensor_values[3]; //Compare two central sensor values
	for (int i=0; i<4; i+=1){
		error += sensor_weights[i] * (sensor_values[7-i] - sensor_values[i]); //Compare pairs of sensor values
	}
	return error;
}

void NeuralNetworkInterface::set_current_interface(NeuralNetworkInterface* subclass){
	interface = subclass;
}

void NeuralNetworkInterface::image_callback(cv_bridge::CvImageConstPtr cv_ptr, bool is_colour){
	//auto start = std::chrono::high_resolution_clock::now();
	//cout << typeid(msg).name()<<endl;
	//const sensor_msgs::ImageConstPtr& newmsg = msg;
	
	cv::Mat_<float_t> input_img = cv_ptr->image;
	if (is_colour)
		cvtColor(input_img, input_img, CV_BGR2GRAY);
	
	//input_img.convertTo(input_img, CV_64F);
	if (!input_img.isContinuous())
		input_img = input_img.clone();;
	float scale = 1.0/255.0;
	vec_t nn_input;
	transform(input_img.begin(),input_img.end(),back_inserter(nn_input), [=](float_t c) {return 1-(c * scale);}); //normalise input image
	
	if (sensor_values[1]>0.1 || sensor_values[0]>0.1){
		interface->send_command(-1, SPEED);
		cout << "edgel"<<endl;
		return;
	}
	else if (sensor_values[6]>0.1 || sensor_values[7]>0.1){
		interface->send_command(1, SPEED);
		cout << "edger"<<endl;
		return;
	}
	
	float command = interface->predict(nn_input);
	//command = (command>1) ? 1 : command;
	//command = (command<-1) ? -1 : command;
	interface->send_command(command, SPEED);
	
	if(!interface->start_learning){
		if(interface->buff_idx == 2){
			interface->start_learning = true;
		}
		interface->output_buffer[interface->buff_idx] = command;
		interface->update_img_buffer(nn_input);
		interface->buff_idx = (interface->buff_idx+1)%3;
		return;
	}
	
	flag = false;
	float error = calc_error();
	cout << "Command: "<<interface->output_buffer[interface->buff_idx]<<" Label: " << interface->output_buffer[interface->buff_idx] + error <<endl;
	interface->train(error, 3);

	interface->output_buffer[interface->buff_idx] = command;
	interface->update_img_buffer(nn_input);
	interface->buff_idx = (interface->buff_idx+1)%3;
	
	/*
	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = finish -start;
	cout<<"Time: " <<elapsed.count()<<endl;
	*/
	//double *nn_image = input_img.ptr<double>(0);
	/*	
	this->neural_net->setInputs(nn_image);
	this->neural_net->propInputs();
	this->neural_net->setLocalError(0);
	this->neural_net->propGlobalErrorBackwardLocally();
	*/
	
}
	
void NeuralNetworkInterface::send_command(float command, float speed){
	cout<<"COMMAND: " << command <<endl;
	geometry_msgs::Twist motors_msg;
	motors_msg.linear.x = speed;
	motors_msg.angular.z = command;
	this->publish_motor(motors_msg);
	
}

auto cbtime = std::chrono::high_resolution_clock::now();
bool started = false;
void callback_img_test(const sensor_msgs::ImageConstPtr& msg){
	auto finish = std::chrono::high_resolution_clock::now();
	if (started){
		std::chrono::duration<double> elapsed = finish -cbtime;
		cout << "Time: " << elapsed.count() <<endl;
	}else{
		started = true;
	}
	cbtime = finish;
}

	

