import subprocess
import sys
import signal
import time
import os
import pprint
import json

from contextlib import contextmanager
from subprocess import Popen, PIPE, STDOUT
from os import path
from time import sleep

# default port for the  server
port = "12345"

# default IP for the server
ip = "127.0.0.1"

# default UDP client path
udp_client_path = "pcom_hw2_udp_client"

# default size of test output line
test_output_line_size = 40

####### Test utils #######
# dictionary containing test IDs and their statuses
tests = {
  "compile": "not executed",
  "server_start": "not executed",
  "c1_start": "not executed",
  "data_unsubscribed": "not executed",
  "c1_subscribe_all": "not executed",
  "data_subscribed": "not executed",
  "c1_stop": "not executed",
  "c1_restart": "not executed",
  "data_no_clients": "not executed",
  "same_id": "not executed",
  "c2_start": "not executed",
  "c2_subscribe": "not executed",
  "c2_subscribe_sf": "not executed",
  "data_no_sf": "not executed",
  "data_sf": "not executed",
  "c2_stop": "not executed",
  "data_no_sf_2": "not executed",
  "data_sf_2": "not executed",
  "c2_restart_sf": "not executed",
  "quick_flow": "not executed",
  "server_stop": "not executed",
}

def pass_test(test):
  """Marks a test as passed."""
  tests[test] = "passed"

def fail_test(test):
  """Marks a test as failed."""
  tests[test] = "failed"

def print_test_results():
  """Prints the results for all the tests."""
  print("")
  print("RESULTS")
  print("-------")
  for test in tests:
    dots = test_output_line_size - len(test) - len(tests.get(test))
    print(test, end="")
    print('.' * dots, end="")
    print(tests.get(test))

####### Topic utils #######
class Topic:
  """Class that represents a subscription topic with data."""

  def __init__(self, name, category, value):
    self.name = name
    self.category = category
    self.value = value

  def print(self):
    """Prints the current topic and data in the expected format."""
    return self.name + " - " + self.category + " - " + self.value

  @staticmethod
  def generate_topics():
    """Generates topics with data for various kinds."""
    ret = []
    ret.append(Topic("a_non_negative_int", "INT", "10"))
    ret.append(Topic("a_negative_int", "INT", "-10"))
    ret.append(Topic("a_larger_value", "INT", "1234567890"))
    ret.append(Topic("a_large_negative_value", "INT", "-1234567890"))
    ret.append(Topic("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwx", "INT", "10"))
    ret.append(Topic("that_is_small_short_real", "SHORT_REAL", "2.30"))
    ret.append(Topic("that_is_big_short_real", "SHORT_REAL", "655.05"))
    ret.append(Topic("that_is_integer_short_real", "SHORT_REAL", "17"))
    ret.append(Topic("float_seventeen", "FLOAT", "17"))
    ret.append(Topic("float_minus_seventeen", "FLOAT", "-17"))
    ret.append(Topic("a_strange_float", "FLOAT", "1234.4321"))
    ret.append(Topic("a_negative_strange_float", "FLOAT", "-1234.4321"))
    ret.append(Topic("a_subunitary_float", "FLOAT", "0.042"))
    ret.append(Topic("a_negative_subunitary_float", "FLOAT", "-0.042"))
    ret.append(Topic("ana_string_announce", "STRING", "Ana are mere"))
    ret.append(Topic("huge_string", "STRING", "abcdefghijklmnopqrstuvwxyz"))
    return ret

####### Process utils#######
class Process:
  """Class that represents a process which can be controlled."""

  def __init__(self, command, cwd=""):
    self.command = command
    self.started = False
    self.cwd = cwd

  def start(self):
    """Starts the process."""
    try:
      if self.cwd == "":
        self.proc = Popen(self.command, universal_newlines=True, stdin=PIPE, stdout=PIPE, stderr=PIPE)
      else:
        self.proc = Popen(self.command, universal_newlines=True, stdin=PIPE, stdout=PIPE, stderr=PIPE, cwd=self.cwd)
      self.started = True
    except FileNotFoundError as e:
      print(e)
      quit()

  def finish(self):
    """Terminates the process and waits for it to finish."""
    if self.started:
      self.proc.terminate()
      self.proc.wait(timeout=1)
      self.started = False

  def send_input(self, proc_in):
    """Sends input and a newline to the process."""
    if self.started:
      self.proc.stdin.write(proc_in + "\n")
      self.proc.stdin.flush()

  def get_output(self):
    """Gets one line of output from the process."""
    if self.started:
      return self.proc.stdout.readline()
    else:
      return ""

  def get_output_timeout(self, tout):
    """Tries to get one line of output from the process with a timeout."""
    if self.started:
      with timeout(tout):
        try:
          return self.proc.stdout.readline()
        except TimeoutError as e:
          return "timeout"
    else:
      return ""

  def get_error(self):
    """Gets one line of stderr from the process."""
    if self.started:
      return self.proc.stderr.readline()
    else:
      return ""

  def get_error_timeout(self, tout):
    """Tries to get one line of stderr from the process with a timeout."""
    if self.started:
      with timeout(tout):
        try:
          return self.proc.stderr.readline()
        except TimeoutError as e:
          return "timeout"
    else:
      return ""

  def is_alive(self):
    """Checks if the process is alive."""
    if self.started:
      return self.proc.poll() is None
    else:
      return False

####### Helper functions #######
@contextmanager
def timeout(time):
  """Raises a TimeoutError after a duration specified in seconds."""
  signal.signal(signal.SIGALRM, raise_timeout)
  signal.alarm(time)

  try:
    yield
  except TimeoutError:
    pass
  finally:
    signal.signal(signal.SIGALRM, signal.SIG_IGN)

def raise_timeout(signum, frame):
  """Raises a TimeoutError."""
  raise TimeoutError

def make_target(target):
  """Runs a makefile for a given target."""
  subprocess.run(["make " + target], shell=True)
  return path.exists(target)

def make_clean():
  """Runs the clean target in a makefile."""
  subprocess.run(["make clean"], shell=True)

def exit_if_condition(condition, message):
  """Exits and prints the test results if a condition is true."""
  if condition:
    print(message)
    make_clean()
    print_test_results()
    quit()

def get_procfs_values(rmem):
  """Reads TCP buffer sizes from procfs."""
  path = "/proc/sys/net/ipv4/tcp_" + ("rmem" if rmem else "wmem")
  #path = "tcp_" + ("rmem" if rmem else "wmem")
  file = open(path, "r")
  values = file.readline().split()
  if len(values) < 3:
    print("Error: could not read correctly from procfs")
    return ["error"]

  return values

def set_procfs_values(rmem, values):
  """Writes TCP buffer sizes to procfs."""
  path = "/proc/sys/net/ipv4/tcp_" + ("rmem" if rmem else "wmem")
  #path = "tcp_" + ("rmem" if rmem else "wmem")

  if not os.access(path, os.W_OK):
    print("Error: not enough permissions to write to procfs")
    return False

  file = open(path, "w")
  file.write(values[0] + " " + values[1] + " " + values[2])

  return True

####### Test helper functions #######
def run_udp_client(mode=True, type="0"):
  """Runs a UDP client which generates messages on one or multiple topics."""
  if mode:
    udpcl = Process(["python3", "udp_client.py", ip, port], udp_client_path)
    udpcl.start()
    for i in range(19):
      outudp = udpcl.get_output_timeout(1)
    udpcl.finish()
  else:
    udpcl = Process(["python3", "udp_client.py", "--mode", "manual", ip, port], udp_client_path)
    udpcl.start()
    sleep(1)
    udpcl.send_input(type)
    sleep(1)
    udpcl.send_input("exit")
    udpcl.finish()

def start_and_check_client(server, id, restart=False, test=True):
  """Starts a TCP client and checks that it starts."""
  if test:
    fail_test("c" + id + ("_restart" if restart else "_start"))

  print("Starting subscriber C" + id)
  client = Process(["./subscriber", "C" + id, ip, port])
  client.start()
  sleep(1)
  outs = server.get_output_timeout(2)

  success = True

  # check if the client successfully connected to the server
  if not client.is_alive():
    print("Error: subscriber C" + id + " is not up")
    success = False

  if not outs.startswith("New client C" + id + " connected from"):
    print("Error: server did not print that C" + id + " is connected")
    success = False
  
  if success and test:
    pass_test("c" + id + ("_restart" if restart else "_start"))

  return client, success

def check_subscriber_output(c, client_id, target):
  """Compares the output of a TCP client with an expected string."""
  outc = c.get_output_timeout(1)
  if target not in outc:
    print("Error: C" + client_id + " output should contain [" + target + "], is actually [" + outc.rstrip() + "]")
    return False

  return True

def check_subscriber_stop(server, c, id):
  """Stops a TCP client and checks that it stops."""
  print("Disconnecting subscriber C" + id)
  c.send_input("exit")
  sleep(1)

  # check that the process is no longer alive
  outs = server.get_output_timeout(1)
  message = "Client C" + id + " disconnected."
  if outs.rstrip() != message or c.is_alive():
    print("Error: client C" + id + " not disconnected")
    return False

  return True

def check_two_subscribers(c1, c2, topics, topic_id):
  """Compares the output of two TCP clients with an expected string."""
  topic = topics[topic_id]

  # generate one message for the topic
  print("Generating one message for topic " + topic.name)
  run_udp_client(False, str(topic_id))

  # check that both subscribers receive the message correctly
  target = topic.print()
  success = check_subscriber_output(c1, "1", target)
  return check_subscriber_output(c2, "2", target) and success

####### Test functions #######
def run_test_compile():
  """Tests that the server and subscriber compile."""
  fail_test("compile")
  print("Compiling")
  exit_if_condition(not make_target("server"), "Error: server could not be built")
  exit_if_condition(not make_target("subscriber"), "Error: subscriber could not be built")
  pass_test("compile")

def run_test_server_start():
  """Tests that the server starts correctly."""
  fail_test("server_start")
  print("Starting the server")
  server = Process(["./server", port])
  server.start()
  sleep(1)
  exit_if_condition(not server.is_alive(), "Error: server is not up")
  pass_test("server_start")
  return server

def run_test_c1_start(server):
  """Tests that a subscriber C1 starts correctly."""
  return start_and_check_client(server, "1")

def run_test_data_unsubscribed(server, c1):
  """Tests that messages from topics not subscribed to are not received."""
  fail_test("data_unsubscribed")

  # generate one message for each topic
  print("Generating one message for each topic")
  run_udp_client()

  # check that the server and C1 print nothing
  outs = server.get_output_timeout(1)
  outc1 = c1.get_output_timeout(1)

  failed = False

  if outs != "timeout":
    print("Error: server printing [" + outs.rstrip() + "]")
    failed = True

  if outc1 != "timeout":
    print("Error: C1 printing [" + outc1.rstrip() + "]")
    failed = True

  if not failed:
    pass_test("data_unsubscribed")

def run_test_c1_subscribe_all(server, c1, topics):
  """Tests that subscriber C1 can subscribe to all topics."""
  fail_test("c1_subscribe_all")
  print("Subscribing C1 to all topics without SF")

  failed = False
  for topic in topics:
    c1.send_input("subscribe " + topic.name + " 0")
    outc1 = c1.get_output_timeout(1)
    if not outc1.startswith("Subscribed to topic."):
      print("Error: C1 not subscribed to all topics")
      failed = True
      break

  if not failed:
    pass_test("c1_subscribe_all")

def run_test_data_subscribed(server, c1, topics):
  """Tests that subscriber C1 receives messages on subscribed topics."""
  fail_test("data_subscribed")

  # generate one message for each topic
  print("Generating one message for each topic")
  run_udp_client()

  # check that C1 receives all the messages correctly
  success = True
  for topic in topics:
    success = check_subscriber_output(c1, "1", topic.print()) and success

  if success:
    pass_test("data_subscribed")

def run_test_c1_stop(server, c1):
  """Tests that subscriber C1 stops correctly."""
  fail_test("c1_stop")

  if check_subscriber_stop(server, c1, "1"):
    pass_test("c1_stop")
    return True

  return False

def run_test_c1_restart(server):
  """Tests that subscriber C1 restarts correctly."""

  # generate one message for each topic
  print("Generating one message for each topic")
  run_udp_client()
  sleep(1)

  # restart and check subscriber C1
  return start_and_check_client(server, "1", True)

def run_test_data_no_clients(c1):
  """Tests that subscriber C1 doesn't receive anything from the server upon restart."""
  fail_test("data_no_clients")

  if c1.get_output_timeout(1) == "timeout":
    pass_test("data_no_clients")

def run_test_same_id(server):
  """Tests that the server doesn't accept two subscribers with the same ID."""
  fail_test("same_id")
  print("Starting another subscriber with ID C1")

  c1bis = Process(["./subscriber", "C1", ip, port])
  c1bis.start()
  sleep(1)

  outs = server.get_output_timeout(2)
  success = True

  if c1bis.is_alive():
    print("Error: second subscriber C1 is up")
    success = False

  if not outs.startswith("Client C1 already connected."):
    print("Error: server did not print that C1 is already connected")
    success = False

  if success:
    pass_test("same_id")

def run_test_c2_start(server):
  """Tests that a subscriber C2 starts correctly."""
  return start_and_check_client(server, "2")

def run_test_c2_subscribe(c2, topics):
  """Tests that subscriber C2 can subscribe to a topic."""
  fail_test("c2_subscribe")
  topic = topics[0]

  print("Subscribing C2 to topic " + topic.name + " without SF")

  c2.send_input("subscribe " + topic.name + " 0")
  outc2 = c2.get_output_timeout(1)
  if not outc2.startswith("Subscribed to topic."):
    print("Error: C2 not subscribed to topic " + topic.name)
    return

  pass_test("c2_subscribe")

def run_test_c2_subscribe_sf(c2, topics):
  """Tests that subscriber C2 can subscribe to a topic with SF."""
  fail_test("c2_subscribe_sf")
  topic = topics[1]

  print("Subscribing C2 to topic " + topic.name + " with SF")

  c2.send_input("subscribe " + topic.name + " 1")
  outc2 = c2.get_output_timeout(1)
  if not outc2.startswith("Subscribed to topic."):
    print("Error: C2 not subscribed to topic " + topic.name)
    return

  pass_test("c2_subscribe_sf")

def run_test_data_no_sf(c1, c2, topics):
  """Tests that subscribers C1 and C2 receive messages on a subscribed topic."""
  fail_test("data_no_sf")

  if check_two_subscribers(c1, c2, topics, 0):
    pass_test("data_no_sf")

def run_test_data_sf(c1, c2, topics):
  """Tests that subscribers C1 and C2 receive messages on a subscribed topic with SF."""
  fail_test("data_sf")

  if check_two_subscribers(c1, c2, topics, 1):
    pass_test("data_sf")

def run_test_c2_stop(server, c2):
  """Tests that subscriber C2 stops correctly."""
  fail_test("c2_stop")

  if check_subscriber_stop(server, c2, "2"):
    pass_test("c2_stop")
    return True

  return False

def run_test_data_no_sf_2(c1, topics):
  """Tests that subscriber C1 receive a message on a subscribed topic."""
  fail_test("data_no_sf_2")
  topic = topics[0]

  # generate one message for the non-SF topic
  print("Generating one message for topic " + topic.name)
  run_udp_client(False, "0")

  # check that C1 receives the message correctly
  if check_subscriber_output(c1, "1", topic.print()):
    pass_test("data_no_sf_2")

def run_test_data_sf_2(c1, topics):
  """Tests that subscriber C1 receive three messages on a subscribed topic with SF."""
  topic = topics[1]
  fail_test("data_sf_2")

  # generate three messages for the SF topic
  print("Generating three messages for topic " + topic.name)

  success = True  
  for i in range(3):
    run_udp_client(False, "1")

    # check that C1 receives the message correctly
    success = check_subscriber_output(c1, "1", topic.print()) and success

  if success:
    pass_test("data_sf_2")

def run_test_c2_restart_sf(server, topics):
  """Tests that subscriber C2 receives missed SF messages upon restart."""
  fail_test("c2_restart_sf")
  topic = topics[1]

  # restart and check subscriber C2
  c2, success = start_and_check_client(server, "2", True, False)

  if success:
    # check that all three SF messages are properly received
    ok = True  
    for i in range(3):
      ok = check_subscriber_output(c2, "2", topic.print()) and ok

    if ok:
      pass_test("c2_restart_sf")

  return c2

def run_test_quick_flow(c1, topics):
  """Tests that subscriber C1 receives many messages in quick succession on subscribed topics."""
  fail_test("quick_flow")

  rmem = get_procfs_values(True)
  wmem = get_procfs_values(False)

  if rmem[0] == "error" or wmem[0] == "error":
    return

  if not set_procfs_values(True, ["5", "5", "5"]):
    return
  
  if not set_procfs_values(False, ["5", "5", "5"]):
    set_procfs_values(True, rmem)
    return

  # generate one message for each topic 30 times in a row
  print("Generating one message for each topic 30 times in a row")
  for i in range(30):
    run_udp_client()

  # check that C1 receives all the messages correctly
  success = True
  for i in range(30):
    for topic in topics:
      success = check_subscriber_output(c1, "1", topic.print()) and success

  if success:
    pass_test("quick_flow")

  set_procfs_values(True, rmem)
  set_procfs_values(False, wmem)

def run_test_server_stop(server, c1):
  """Tests that the server stops correctly."""
  fail_test("server_stop")
  print("Stopping the server")

  server.send_input("exit")
  sleep(1)

  success = True

  if server.is_alive():
    print("Error: server is still up")
    success = False

  if c1.is_alive():
    print("Error: C1 is still up")
    success = False

  if success:
    pass_test("server_stop")

def h2_test():
  """Runs all the tests."""

  # clean up
  make_clean()

  # generate the topics
  topics = Topic.generate_topics()

  # build the two binaries and check
  run_test_compile()

  # start the server and check it is running
  server = run_test_server_start()

  # start a subscriber C1 and check it is running
  c1, success = run_test_c1_start(server)

  if success:    
    # generate data and check that it isn't received by C1
    run_test_data_unsubscribed(server, c1)

    # subscribe C1 to all topics and verify
    run_test_c1_subscribe_all(server, c1, topics)

    # generate messages on all topics and check that C1 receives them
    run_test_data_subscribed(server, c1, topics)

    # stop C1 and check it exits correctly
    success = run_test_c1_stop(server, c1)

    if success:
      # restart C1 and check that it starts properly
      c1, success = run_test_c1_restart(server)

      if success:
        # check that C1 doesn't receive anything from the server
        run_test_data_no_clients(c1)

        # connect a client with the same ID as C1 and check that it fails
        run_test_same_id(server)

        # start a subscriber C2 and check it is running
        c2, success = run_test_c2_start(server)

        if success:
          # subscribe C2 to a single non-SF topic and check
          run_test_c2_subscribe(c2, topics)

          # subscribe C2 to a single SF topic and check
          run_test_c2_subscribe_sf(c2, topics)

          # generate a message on the non-SF topic and check
          run_test_data_no_sf(c1, c2, topics)

          # generate a message on the SF topic and check
          run_test_data_sf(c1, c2, topics)

          # stop C2 and check it exits correctly
          success = run_test_c2_stop(server, c2)

          if success:
            # generate a message on the non-SF topic and check
            run_test_data_no_sf_2(c1, topics)

            # generate three messages on the non-SF topic and check
            run_test_data_sf_2(c1, topics)

            # restart C2 and check that all SF messages are received
            c2 = run_test_c2_restart_sf(server, topics)

    # send all types of message 30 times in quick succesion and check
    run_test_quick_flow(c1, topics)

  # close the server and check that C1 also closes
  run_test_server_stop(server, c1)

  # clean up
  make_clean()

  # print test results
  print_test_results()

# run all tests
h2_test()
