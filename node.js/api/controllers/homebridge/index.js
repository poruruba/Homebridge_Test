'use strict';

const HELPER_BASE = process.env.HELPER_BASE || '../../helpers/';

var status_light = {
  On: false,
  HSV: [0, 0, 0]
};

var status_switch = {
  On: false,
  readonly: true
};

var status_key = {
  State: 'L',
  readonly: false
};

var status_pir = {
  PIR : 0
}

function switch_publishOnOff(mqtt){
  mqtt.publish('switch/getOn', status_switch.On ? "true" : "false");
}

function key_publish(mqtt){
  mqtt.publish('key/getLockCurrentState', status_key.State);
}

function light_publishOnOff(mqtt) {
  mqtt.publish('light/getOn', status_light.On ? "true" : "false");
}

function light_publish(mqtt) {
  mqtt.publish('light/getHSV', status_light.HSV.map(item => Number(item)).join(','));
}

function motion_notify(mqtt){
  mqtt.publish('motion/getMotionDetected', (status_pir.PIR == 0) ? "true" : "false");
}

exports.handler = async (event, context) => {
  console.log(event);
  console.log(context);

  if (context.topic == "key/startup") {
    key_publish(context.mqtt);
  } else
  if( context.topic == 'light/startup'){
    light_publishOnOff(context.mqtt);
    light_publish(context.mqtt);
  }else
  if( context.topic == 'switch/startup'){
    switch_publishOnOff(context.mqtt);
  }else
  if( context.topic == "light/setOn" ){
    status_light.On = (event == "true") ? true : false;

    light_publishOnOff(context.mqtt);
  }else
  if( context.topic == "light/setHSV" ){
    status_light.HSV = event.split(',').map(item => parseInt(item));

    light_publish(context.mqtt);
  }else
  if( context.topic == "switch/setOn" ){
    if( !status_switch.readonly )
      status_switch.On = (event == "true") ? true : false;

    switch_publishOnOff(context.mqtt);
  }else
  if( context.topic == "key/setLockTargetState" ){
    if( !status_key.readonly )
      status_key.State = event;
    
    key_publish(context.mqtt);
  }
};
