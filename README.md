Author: Ryan Kelly

Class: Computer Science 4760 - Operating Systems

School: University of Missouri - Saint Louis

Assignment: Project 6

Due Date: 5/5/2022

Language Used: C

Description: In this project we will be designing and implementing a memory management module for our Operating System Simulator oss. In particular, we will be implementing the FIFO (clock) page replacement algorithms. When a page-fault occurs, it will be necessary to swap in that page. If there are no empty frames, your algorithm will select the victim frame based on our FIFO replacement policy. This treats the frames as one large circular queue. Each frame should also have an additional dirty bit, which is set on writing to the frame. This bit is necessary to consider dirty bit optimization when determining how much time these operations take. The dirty bit will be implemented as a part of the page table.

Build Instructions (Linux Terminal): make

Delete oss and process Executables and logfile: make clean

How to Invoke: ./oss

GitHub: https://github.com/rwk2f6/cs4760_p6

Issues: My project isn't able to fill the page table, and the print_mem function seems to have a bug where it doesn't properly display if a frame is filled or not. If you look at my debug lines, printfs, it will print the frame location and address it is trying to set right before actually setting them. It shows up fine in the printf but only sometimes do the frames actually get set.