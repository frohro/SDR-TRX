
import socket

def send_udp_message(ip_address, port, message):
    """
    Sends a UDP message to the specified IP address and port.

    Args:
        ip_address (str): The IP address to send the message to.
        port (int): The port number to send the message to.
        message (str): The message to send.

    Returns:
        str: The response received from the server.
    """
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Send the message
    message = message.encode('utf-8')
    sock.sendto(message, (ip_address, port))

    # Receive the response
    response, address = sock.recvfrom(1024)

    # Close the socket
    sock.close()

    # Return the response
    return response.decode('utf-8')

# Define the IP address and port
ip_address = '192.168.1.104'
port = 12346

# Send the message and print the response
message = 'VER\n'
response = send_udp_message(ip_address, port, message)
print(response)
