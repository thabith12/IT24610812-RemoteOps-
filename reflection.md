# Reflection

Developing the RemoteOps system gave me practical experience in network programming and helped me understand how TCP/IP communication works beyond theoretical concepts. Before implementing the project, I mainly understood TCP and UDP from networking lessons. During this assignment, I learned how these concepts are applied using C sockets, including socket creation, binding, listening, accepting connections, sending and receiving data, and closing connections correctly.

One of the main challenges was handling TCP communication correctly. I initially had to understand that TCP is a byte stream and that one recv() call does not necessarily represent one complete command. Implementing newline-based command reception helped me understand TCP stream framing more clearly. File transfer was another important learning area because PUT and GET required handling exact byte counts rather than relying only on text-based messages.

The concurrency requirement also improved my understanding of POSIX threads. Implementing a thread-per-Controller model showed me how a server can handle multiple clients independently. Testing the Agent with five simultaneous Controller connections provided practical evidence that the concurrency design was working.

Security was another important part of the project. Implementing token-based authentication, restricting EXEC commands using a whitelist, validating filenames, and applying a maximum file-size limit helped me understand basic security considerations in network applications.

The UDP monitoring feature gave me experience with connectionless communication and periodic monitoring data. I also learned the importance of logging and testing when developing a network application. Using timestamps in the log file made it easier to verify the sequence of operations.

The optional throughput measurement and file integrity testing using cmp and SHA-256 provided additional experience in evaluating file-transfer performance and correctness.

Overall, this project improved my confidence in C socket programming, TCP/IP networking, concurrency, file transfer, authentication, monitoring, debugging, and Git-based development. It also helped me understand how different networking concepts can be combined to build a practical client-server application.
