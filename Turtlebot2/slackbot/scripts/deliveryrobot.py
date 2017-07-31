#!/usr/bin/env python
import os
import time
from slackclient import SlackClient
from nltk.tag import pos_tag

import rospy
from std_msgs.msg import String

import setNavGoal as ng

# bot's ID as an environment variable
BOT_ID = os.environ.get("BOT_ID")
AT_BOT = "<@" + BOT_ID + ">"
slack_client = SlackClient(os.environ.get('SLACK_BOT_TOKEN'))
global curr_item

def callback(data):
	#print(data)
	global curr_item
	curr_item = data

'''def sendItem(item):
	pub = rospy.Publisher('slackitem', String, queue_size=10)
	rate = rospy.Rate(10) # 10hz
	pub.publish(str(item))'''

def sendGoal(loc):
	try:
		rospy.init_node('Goal', anonymous=False)

		#clearCostMap()
		
		nav = ng.GoToPose()
	
		position = {'x': loc[0], 'y' : loc[1]}
	        quaternion = {'r1' : loc[2], 'r2' : loc[3], 'r3' : loc[4], 'r4' : loc[5]}
	
		success = nav.goto(position,quaternion)
	
		if success:
		    rospy.loginfo("Reached destination")
	        else:
	            rospy.loginfo("FAILED")
	
	except rospy.ROSInterruptException:
        	rospy.loginfo("Ctrl-C caught. Quitting")

	

'''**************
Slack bot methods
**************'''

def handle_command(command, channel, points, location = False):
    response = "Oh so you want a "
    try:
        tagged_sent = pos_tag(command.split())
        propernouns = [word for word,pos in tagged_sent if pos == 'NN']
        if len(propernouns) is not 0: response+=str(propernouns[len(propernouns)-1])
        response+="! Is that correct?"
        if 'ye' in command and 'yel' not in command:
            print "yo"
            response = "Cool! What's your location?"
            slack_client.api_call("chat.postMessage", channel=channel,
                                  text=response, as_user=True)
            return True, ""
        elif "no" in command:
            response = "Oh ok. What do you want then?"
            slack_client.api_call("chat.postMessage", channel=channel,
                                  text=response, as_user=True)
            return False, ""

        if len(command.split()) is 1 and "Sorry" not in response and "Cool" not in response and "Is that correct" not in response:
            response = "please use more than one word to tell me what you want. I.E. get me some salad"
    except ValueError:
        response = "Sorry! I'm confused. Please tell me what you want again."
    if location:
        try:
	    slack_client.api_call("chat.postMessage", channel=channel,
                          text="Thanks! Your order will soon be delivered", as_user=True)
            sendGoal(point_list[command])
        except rospy.ROSInterruptException:
            pass
        response = "Your order has been delivered!"
    slack_client.api_call("chat.postMessage", channel=channel,
                          text=response, as_user=True)
    return False, str(propernouns[len(propernouns)-1])


def parse_slack_output(slack_rtm_output):
    output_list = slack_rtm_output
    if output_list and len(output_list) > 0:
        for output in output_list:
            if output and 'text' in output and AT_BOT in output['text']:
                # return text after the @ mention, whitespace removed
                return output['text'].split(AT_BOT)[1].strip(), \
                       output['channel']
    return None, None

def clearCostMap():
	rospy.wait_for_service('move_base/clear_costmaps')
	try: 
		clear_costmaps = rospy.ServiceProxy('move_base/clear_costmaps',Empty)
	except rospy.ServiceException, e:
		print "Service call failed: %s"%e

if __name__ == "__main__":
    rospy.init_node('slackbot', anonymous=True)
    READ_WEBSOCKET_DELAY = 1 # 1 second delay between reading from firehose
    f = open('points.txt', 'r')
    point_list = eval(str('{')+f.readline()+str('}'))
    print(point_list)
    if slack_client.rtm_connect():
        print("Bot connected and running!")
        is_loc = False
	item = ""
	temp_item =''
        while True:
            command, channel = parse_slack_output(slack_client.rtm_read())
            if command and channel:
		temp_item = item
                is_loc, item = handle_command(command, channel, point_list, location = is_loc)
		print"gfd"
		print "FDSA"
	    if len(item) <= 1: item = temp_item
	    rospy.Subscriber("item", String, callback)
	    publ = rospy.Publisher('smartmove', String, queue_size=10)
	    rate = rospy.Rate(10) # 10hz
	    if('curr_item' in globals()):
		    try:
			    global curr_item
			    print("item "+str(item))
			    print("curr_item "+str(curr_item))
			    if (item in str(curr_item)) and ('1' in str(curr_item)):
			    	publ.publish(str("go"))
			    else:
				publ.publish(str("stop"))
		    except:
			   curr_item=" "
            time.sleep(READ_WEBSOCKET_DELAY)
    else:
        print("Connection failed. Invalid Slack token or bot ID?")
