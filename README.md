# Multiprocess Instant Messaging Application

## Project Overview

This is a C-based multiprocess instant messaging application designed to run on Linux. Users can log in simultaneously and exchange messages either through a server or via peer-to-peer communication. The application leverages socket programming (TCP), `select()`, and process control to manage multiple client connections.

### Features

- **Multi-user login**: Multiple users can log in simultaneously using the same device executable.
- **Messaging**: Users can send and receive messages via a server or directly peer-to-peer when the server goes offline.
- **Offline message storage**: Messages are saved to a text file when a user is offline and can be retrieved once they log back in.
- **Contact list**: Users can access their contact list to see who they can message.
- **File storage**: Each user has a personal directory for storing messages and contacts in text files.
- **Server and peer-to-peer modes**: The application starts by connecting to a central server but can seamlessly switch to peer-to-peer communication if the server is disconnected.

## Technical Details

- **Language**: C
- **Platform**: Linux
- **Communication Protocol**: TCP
- **Sockets and IPC**: The application makes extensive use of C sockets (`accept()`, `bind()`, `select()`, etc.) for managing connections and inter-process communication.

## How to Build and Run the Application

1. **Compile the Application**:
   Ensure you are in the root directory where the `Makefile` is located, and then run the following command:

   ```bash
   make
   ```

   This will compile both the **server** and **device** executables.

2. **Run the Server**:
   Start the server by running:

   ```bash
   ./server
   ```

3. **Run the Client (Device)**:
   Start a client session (device) by running:

   ```bash
   ./device
   ```

4. **Login or Signup**:

   - After starting the client, the application prompts each user to log in or sign up with a unique username.

5. **Send Messages**:

   - Once logged in, users can exchange messages by selecting contacts from their contact list.
   - Messages are saved when users are offline and retrieved upon their next login.

6. **Peer-to-Peer Mode**:

   - If the server is shut down, users who are already connected can continue messaging in peer-to-peer mode.

7. **Clean up**:
   To remove compiled files, you can run the `clean` command:
   ```bash
   make clean
   ```

## Directory Structure

- **User contact lists**: Each user has a `.txt` file storing their contact list.
- **Message logs**: Messages between users are saved as logs, with files named after the participating users (e.g., `user1+user2-chat.txt`).
- **Offline messages**: Messages sent to offline users are stored in separate files and delivered when they reconnect.

## Prerequisites

- A Linux environment.
- GCC and Make installed for compiling the C programs.

## Files

- **server.c**: Server-side code that manages the central messaging server.
- **device.c**: Client-side code that handles user login, messaging, and peer-to-peer communication.
- **Makefile**: Script to compile the `device` and `server` programs and clean up the build.

## Documentation

For more details on the project's structure and implementation, refer to the [full documentation](Documentazione.pdf).
