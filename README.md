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

Issues: