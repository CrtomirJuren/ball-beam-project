/*
 * PID Ball Beam Project 
 * 
 * 
 */
//-----------------------------------------------------
//--------LIBRARIES, INCLUDES, HEADERS-----------------
//-----------------------------------------------------
#include "Adafruit_VL53L0X.h"

#include <MovingAverage.h>
#include <FastPID.h>
#include <Servo.h>

//-----------------------------------------------------
//-------------------DEFINES---------------------------
//-----------------------------------------------------
//#define SERVO_MOTOR_PIN     9
//#define PIN_SETPOINT  A1
//#define PIN_OUTPUT    9

//-----------------------------------------------------
//---------------GLOBAL VARIABLES----------------------
//-----------------------------------------------------

//--------------SERVO------------
const int servo_motor_pin = 9;
int angle_limit = 30;                 //limits relative to beam cca +/-30 degrees
int servo_offset = 91;                //horizontal beam position is angle 0

int servo_sp;                        // variable to store the servo position
//int servo_sp; 

//----------------------------------------

//MAIN PID PROCESS VARIABLES
float pid_pv_nofilter = 0;            //noisy process variable = distance[mm]
float pid_pv_filter = 0;              //filtered process variable = distance[mm]
float pid_sp = 100;                   //setpoint = distance[mm]
float pid_mv = 0;                     //manipulated variable -100 < mv < -100%
float pid_err = 0;                   //error = setpoint - process variable
float pid_err_old;
float pid_err_diff = 0;

//deluje -> kp = 0.5, ki = 0, kd = 4, 
//deluje -> kp = 0.2, ki = 0, kd = 4, 

//float pid_kp=0.5, pid_ki=0.05, pid_kd=10; boljše

//float pid_kp=0.5, pid_ki=0, pid_kd=6; hitrejše
//float pid_kp=0.5, pid_ki=0.5, pid_kd=6; NE DELUJE ki ZMEDE
//float pid_kp=0.5, pid_ki=0.1, pid_kd=6; DELUJE


float pid_kp=0.5, pid_ki=0.05, pid_kd=5;

float pid_mv_previous; //used in derivate calculation
float proportional, integral, derivate;
 
bool output_signed = false;

int distance = 0;
int distance_avg = 0;

int servo_angle = 0;    // variable to store the servo position


//-----------------------------------------------------
//-----------------FUNCTION DEFINES--------------------
//-----------------------------------------------------

void initialize_serial(void);
void send_serial(void);

//-----------------------------------------------------
//------------OBJECTS INITIALIZATION-------------------
//-----------------------------------------------------
/* Create Moving avarage Object: 
  tpye = unsigned int
  size = 8
  initialize value = 0 */
MovingAverage<unsigned> moving_avg_array(8, 100);

/* Create Laser distance sensor Object: */
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

/* Create servo motor Object: */
Servo myservo; 

//-----------------------------------------------------
//----------------SETUP LOOP---------------------------
//-----------------------------------------------------
void setup()
{
  //initialize serial communication
  initialize_serial();

  //Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println("Failed to boot VL53L0X");
    while(1);
  }
  
  //attaches the servo on pin 9 to the servo object
  myservo.attach(servo_motor_pin);
}

//-----------------------------------------------------
//-----------------MAIN LOOP---------------------------
//-----------------------------------------------------
void loop()
{
  //-------------------------------------
  //-----------GET DISTANCES-------------
  //filter and set pid process variable
  //-------------------------------------
  
  // read distance from sensor VL53L0X
  distance = get_distance();
  
  //add value to moving avarage
  moving_avg_array.push(distance);
  
  pid_pv_nofilter = distance;
  
  //read avarage of moving avarage array
  pid_pv_filter = moving_avg_array.get();
        
  //-------------------------------------
  //------------CALCULATE PID------------
  //-------------------------------------
  //calculate error
  pid_err_old = pid_err;
  pid_err = pid_sp - pid_pv_filter;
  pid_err_diff = pid_err - pid_err_old;

  //P calculate proportional value
  proportional = pid_err * pid_kp;

  //I calculate integral value
  integral = integral + pid_err;
  integral = integral*pid_ki;
  
  //D calculate derivate value
  //derivate = (pid_mv - pid_mv_previous )*pid_kd;
  derivate = pid_kd*pid_err_diff;
  
  pid_mv = proportional + integral + derivate;

  //-------------------------------------
  //--------UPDATE SERVO POSITIONS--------
  //-------------------------------------
  //if pid_mv < 0, motor angle > 90, ball will accelerate towards sensor
  //if pid_mv =0,  motor angle = 90, ball will not accelerate
  //if pid_mv > 0, motor angle < 90, ball will accelerate away from sensor
  pid_mv = proportional + integral + derivate;
  //pid_mv = 0; // -100% < pid_mv< 100%

  set_servo_position(pid_mv); 

  //-------------------------------------
  //--------SEND DATA OVER SERIAL--------
  //-------------------------------------
  send_serial();
  
  //-------------------------------------
  //-----------LOOP TIME DELAY-----------
  //-------------------------------------
  delay(1);
}

//-----------------------------------------------------
//--------------USER FUNCTIONS-------------------------
//-----------------------------------------------------

/*************************************

**************************************/
void initialize_serial(void){
  // set serial speed
  Serial.begin(9600);
  // wait until serial port opens
  while (! Serial) {
    delay(1);
  }
  //------SET ARDUINO SERIAL PLOTTER NAMES------------
  Serial.println("sp,mv,pv_nofilter,pv_filter,err");   
}

/*************************************

**************************************/
void send_serial(void){
  //arduino serial plotter mode
  Serial.print(pid_sp); //  
  Serial.print(","); //
  Serial.print(pid_mv); //  
  Serial.print(","); //
  Serial.print(pid_pv_nofilter); //  
  Serial.print(","); //
  Serial.print(pid_pv_filter); //  
  Serial.print(","); //
  Serial.println(pid_err); //
}

/*************************************
    Returns distance measured from sensor VL53L0X
    @void
    @return distance[mm]
**************************************/
int get_distance(void){
  bool measurement_success;

  // read measurement
  VL53L0X_RangingMeasurementData_t measure;
            
  //Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
  
  // CHECK FOR MEASUREMENT PHASE ERROR
  if(measure.RangeStatus != 4){  // phase failures have incorrect data
    measurement_success = true;
  }
  else{
    measurement_success = false;
  }
  
  // IF MEASUREMENT OK, READ DISTANCE
  if(measurement_success){
    distance = measure.RangeMilliMeter;
  
    //check if distance is over maximum
    if(distance > 300){
      distance = 300;
      //for debbuging, send 
      //Serial.print("max-distance-error");
    }
    return distance; 
  }
  // IF MEASUREMENT NOK, SKIP MEASUREMENT
  else{
  }
}

void set_servo_position(int manipulated_variable){
  // constrain motor output
  servo_sp = map(manipulated_variable, -100, 100, servo_offset - angle_limit, servo_offset + angle_limit);

  //constraint the values for motor and system safety
  servo_sp = constrain(servo_sp, servo_offset - angle_limit, servo_offset + angle_limit); 
  //set_servo_position(servo_sp);
  myservo.write(servo_sp);               
}
