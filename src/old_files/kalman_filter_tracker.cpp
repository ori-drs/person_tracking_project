#include <ros/ros.h> // because this is a robot
// tf includes
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>

#include "include/kalman_filter.h" // for the kalman filter

#include <Eigen/Dense>  // for all the lovely matrices
#include <visualization_msgs/Marker.h>

using namespace std;
using namespace Eigen; // to make matrix stuff more compact

void publish_marker(ros::Publisher vis_pub, VectorXd x_hat,double scale_x,double scale_y,double scale_z) {
    ///
    /// This publishes a box of scales set by vector onto publisher vis_pub at location x_hat
    ///

    visualization_msgs::Marker marker; // initiate the marker

    marker.header.frame_id = "odom"; // we want to publish relative to the odom frame

    marker.header.stamp = ros::Time();
    marker.ns = "kalman_filter_marker";  // call our marker kalman_filter_marker
    marker.id = 0;
    marker.type = visualization_msgs::Marker::CUBE; // make it a cube
    marker.action = visualization_msgs::Marker::ADD;

    // assign the marker location according to x_hat
    marker.pose.position.x = x_hat[0];
    marker.pose.position.y = x_hat[1];
    marker.pose.position.z = x_hat[2];

    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;

    // set the marker size as an input params
    marker.scale.x = scale_x;
    marker.scale.y = scale_y;
    marker.scale.z = scale_z;
    marker.color.a = 0.8; // Don't forget to set the alpha!
    marker.color.r = 0.0;
    marker.color.g = 1.0;
    marker.color.b = 0.0;
    vis_pub.publish( marker );


}

int main (int argc, char** argv)
{
    cout <<"Initialising kalman_filter_tracker"<<endl;
    // 1. Initialise ROS

    ros::init (argc, argv, "multi_sensor_tracker");
    ros::NodeHandle nh;
    ros::Rate r(10); // 10 hz
    // 2. Subscribe to input transform

    tf::TransformListener vel_listener;
    tf::StampedTransform input_tf;

    string base_frame = "odom"; // define the transform origin we are looking for
    string target_frame = "velodyne_person_est"; // define the transform target we are looking for


// kf params

    int n = 3; // Number of states
    int m = 3; // Number of measurements
    VectorXd y(m); // define a variable y to hold all the input transforms. //NOT SURE IF d,m or m,d

    // 3. Create a Kalman filter to process the input

    double dt = 1.0/30; // Time step

    MatrixXd I3(3,3);
    I3.setIdentity();
    cout << I3 << endl;

    MatrixXd A(n, n); // System dynamics matrix
    MatrixXd C(m, n); // Output matrix
    MatrixXd Q(n, n); // Process noise covariance
    MatrixXd R(m, m); // Measurement noise covariance ///////////////////////////////DIMENSION???
    MatrixXd P(n, n); // Estimate error covariance

    // Assuming the person doesn't move, we are JUST X INITIALLY
    A = I3; //I3
    C = I3;
    //C << 1;
    // Reasonable covariance matrices
    //Q << .05, .05, .0, .05, .05, .0, .0, .0, .0; // I DON'T KNOW HOW TO TUNE THIS

    Q << 2, 0, 0, 0, 2, 0, 0, 0, .5; //I3 * .05 // MAKE THIS A FUNCTION OF TIMESTEP^2
    R << 5, 0, 0, 0, 5, 0, 0, 0, 1; //I3 * .05 // MAKE THIS A FUNCTION OF TIMESTEP^2

    //R = I3; //OR THIS // WHAT DIMENSION IS THIS????????????????????///
    //P << .1, .1, .1, .1, 10000, 10, .1, 10, 100; //OR THIS, FOR THAT MATTER
    P << 2, 0, 0, 0, 2, 0, 0, 0, 2; //I3

    //print out the chosen matrices
    cout << "A: \n" << A << endl;
    cout << "C: \n" << C << endl;
    cout << "Q: \n" << Q << endl;
    cout << "R: \n" << R << endl;
    cout << "P: \n" << P << endl;

    // create the filter x
    KalmanFilter kf(dt, A, C, Q, R, P, true);
    VectorXd x0(n);
    VectorXd x_hat(3);
    x0 << 0, 0, 0;
    kf.init(x0); // initialise the kalman filter



    // 4. Create a broadcaster on which to publish the outputs
     tf::TransformBroadcaster br;
     string output_frame = "kalman_filter_est";
     tf::Transform output_tf;
     tf::Quaternion q; // initialise the quaternion q for the pose angle
     q.setEulerZYX(0, 0, 0);
     output_tf.setRotation(q); //set q arbitrarily to 0

    //setup the timers
    ros::Time curr_time, prev_time;
    ros::Duration delta;
    prev_time = ros::Time(0); //init previous time


    // SET A MARKER
    ros::Publisher vis_pub = nh.advertise<visualization_msgs::Marker>( "kalman_filter_marker", 0 ); // this name shows up in RVIZ



//     * 5. Loop:
     while (nh.ok()) {
         // receive the input transform

         try{
           vel_listener.waitForTransform(base_frame, target_frame, ros::Time(0), ros::Duration(5.0) );
           vel_listener.lookupTransform(base_frame, target_frame, ros::Time(0), input_tf);
         }
         catch (tf::TransformException ex){ // for things like- can't see the input transform
           ROS_ERROR("%s",ex.what());
           ros::Duration(1.0).sleep();
         }

         ///////////////////////////////////
         prev_time = curr_time;
         curr_time = ros::Time::now();
         delta = curr_time - prev_time;
         ///////////////////////////////////
         // extract coordinates from transform
         tf::Vector3 input_vector;
         input_vector = input_tf.getOrigin();

         y << input_vector.getX(), input_vector.getY(), input_vector.getZ();
         //y_ << input_vector.getX();// we are only getting one measurement- put this in y
         kf.update(y, delta.toSec(), A);
         x_hat << kf.getState().transpose()[0], kf.getState().transpose()[1],kf.getState().transpose()[2];
         P << kf.getP();
         cout << "P:\n" << P<<endl;

         cout << "t = " << curr_time << ", dt = "<<delta<<", \n    y =" << y.transpose()
             << ",\nx_hat = " << kf.getState().transpose() << endl;

         //Now publish the output
         output_tf.setOrigin( tf::Vector3(x_hat[0],x_hat[1],x_hat[2]) );

         br.sendTransform(tf::StampedTransform(output_tf, ros::Time(0), base_frame, output_frame)); // send the transform

         publish_marker(vis_pub, x_hat, P(0,0),P(1,1),P(2,2)); // publish the marker

         r.sleep();
     }

//     *      publish the output



}
