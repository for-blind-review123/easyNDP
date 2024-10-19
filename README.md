# Cosmos NDP Programming Framework (easyNDP) Documentation

### I. Introduction

This document is primarily intended to explain the architecture, development of new applications, and usage methods of this simplified NDP framework—easyNDP framework.

Before we begin, there is a concept that needs to be clarified in advance. The term "block" in this document corresponds to the block/sector/sector or page/page in the CSD within the host, and does not refer to the erase unit block/block in the flash memory. Typically, a block is 4KB in size, and in Cosmos, a page, which is the block referred to in this document, is 16KB. Therefore, when the block is mentioned in the host, it refers to a 4KB block, and when the block is mentioned in the firmware, it is a 16KB block, which need to be converted in the FTL.

### II. Architecture Introduction

Near-data processing (NDP, Near-data processing), as an emerging computing paradigm, is flourishing. NDP primarily addresses the storage wall issue of traditional storage systems. Unlike traditional systems where data needs to be transported entirely from storage to DRAM, and then to the CPU for computation, NDP systems can perform calculations directly within the storage and return the results. NDP greatly reduces the overhead of communication and the burden on the host CPU, featuring easy scalability, low energy consumption, and high performance.

Storage that can support near-data processing capabilities is called computational storage (CSD, Computational Storage). Due to the performance improvements and cost reduction of flash memory and SSDs, CSD is usually extended from SSDs. The firmware of CSD is usually closed-source (for example, Samsung's SmartSSD, NGD system's Newport, etc.), which is very unfavorable for the development of NDP technology. Even though there are open-source NDP systems (for example, Insider from ATC'19, FusionFS from FAST'22, etc.), they are often not truly implemented on SSD devices but are simulated using FPGAs or on the host side, and cannot be truly applied. Therefore, the purpose of this framework is to break this dilemma, and an easy NDP development framework, easyNDP, has been developed based on the open flash disk Cosmos Plus OpenSSD, facilitating developers and researchers to more conveniently develop NDP applications and conduct research on NDP systems.

easyNDP consists of two parts of code, the host side and the firmware side. The host side is mainly responsible for providing interfaces for applications, and interacting with Cosmos through the NVMe protocol, distributing tasks, and reading and writing data.

The basic principle of easyNDP is to intercept the processing function of data read from the flash memory, and execute a data processing program to achieve NDP operations. The specific process is as follows:

1. The host-side application first constructs the custom instruction structure and data structure that need to be sent down in memory, and then sends down NDP requests through the API. Whether the instruction is blocking depends on the firmware program.
2. After the firmware receives the request, it can first receive the instructions just constructed in the host memory through DMA and then parse out the required information (such as data address, operation type, operation parameters, etc.), and then push the request into the NDP task queue. The firmware continues to accept new requests. The NDP task queue is a new data structure added by easyNDP, mainly used to maintain tasks related to NDP, such as computation, distribution of flash memory operations, aggregation of computation results, etc.
3. At the end of each main loop of the firmware, it will check whether there are any unfinished requests in the NDP task queue. If there are, it will traverse the NDP task queue to find out if there are any completed requests (the overhead is relatively large, and this can be optimized later, for example, by setting up a dedicated task completion queue or canceling this traversal process and adopting a trigger-based completion). If a request is completed, it will return the result and delete the task. If a request is in the initialization state, it will distribute the task.
4. easyNDP has a processing function buried in the flash memory read completion function. Whenever a piece of data is read, it will be processed once. Currently, only the on-the-spot computation mode is supported. After processing is completed, the task's completion status is set to facilitate request processing in step 3.

### III. Application Development and Usage Examples

Here, an application for string search within a text file is used as an example. Here, the file system is used to manage data, and CSD is mounted as a block device on the host side, and file read and write operations can be performed. Here, the example is introduced from top to bottom, that is, from the application to the bottom layer.

#### 1. Host Side (Code in Host Folder)

1. Cosmos first writes a text file into Cosmos by creating or copying, and then synchronizes metadata with `fsync`. Since CSD contains DRAM data cache, the written file is first saved in the cache and not written into the flash memory. Moreover, the firmware (FTL) of Cosmos does not support the operation of flushing the cache, so we need to write a file of tens of MB (the default size of Cosmos data cache is 16MB) to ensure that the file has been completely written into the flash memory.

2. After the file is written, it is necessary to obtain the logical block address of the file. Since there is no file semantic information in CSD, it is impossible to obtain the storage location of the file through the file name, so the host needs to tell CSD where the data is located. The logical block address and physical block address here are for CSD, and the host interacts with CSD using logical block addresses. The FTL in CSD is responsible for converting logical block addresses to physical block addresses. There is a ready-made application called `filefrag` that can obtain the starting block address and file length information of a file. The command format is `filefrag -v -b4096 <file name>`.

3. After obtaining the file address, we fill it in as a parameter into memory. The code for this step is included in `string_demo.c`. We first need to include the `dma.h` header file, which contains the interfaces and underlying code for interacting with Cosmos, based on unvme. Then we apply for a memory area to store the fields we need in order. For example, in step 2, we obtained a file starting logical block number of 33793, a length of 4000B, and occupied 1 block, so we can construct the instruction as follows: 

![Image](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172054001.png)

4. After constructing the NDP instruction, the next step is to send it. Before sending, you need to open the unvme device, and after sending, you need to close it. The NVMe device path is modified in `dma.h`, but it generally does not change and is usually `/dev/nvme0n1`. The sending API is `DMA_Send`, the first parameter is currently not used; the second parameter is the address of the just constructed instruction; the third parameter is the instruction length; the fourth is the instruction operator defined by the application itself, it is recommended that the operator definition is made as a separate header file for easy synchronization between the host side and the firmware side, for example, in this example, various operations are defined in `command_list.h`, and the content of the header file on the firmware side and the host side is exactly the same. 

![Image](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172107423.png)

5. So far, the host-side code development is complete, and it can be compiled.

#### 2. Firmware Side (Code in CSD Folder)

1. On the firmware side, we first need to parse the just received request. The communication principle of NVMe is to first send basic information to the NVMe device through dozens of Bytes of NVMe instructions, and then the required data is accepted or sent through DMA (the DMA address is in the NVMe instruction). The `TEST_C` request type we defined in step 4 on the host side comes into play here. The request type is saved in the reserved field `writeInfo12.reserved0` of the NVMe instruction, and the relevant code is on line 120 of `nvme_io_cmd.c`. After the program parses that it is a `TEST_C` request, it first accepts the request data through DMA, that is, the request data constructed in step 3 on the host side. Line 124 is to initiate dma transmission, and line 125 is to block and wait for dma transmission to complete (it can be designed to be non-blocking, which will be much more complicated because there is no multi-process mechanism inside CSD and you need to manually check the completion status, we will use the blocking acceptance method here first). 

![image-20230217211457777](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172114805.png)

2. After receiving the request, we can parse the required request fields in order, which is not a big focus. However, we can set up a check mechanism to simply check if there are any errors in the data. For example, the block count information and the data byte size here actually have redundancy, so we can check for errors by calculating whether their values match. We can also construct checksums, magic numbers, or sequence numbers in the fields to check for errors.

![image-20230217211445414](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172114439.png)

3. Next, we push this request into the task list `tasklist`. First, we construct a task entity `task_entry` and set all fields to 0 (the `task_entry` is designed such that all fields can be set to 0 to indicate an invalid state). At the same time, we fill in the required field information into the relevant items of `task_entry`. Note that the status item is set to `INIT` to indicate that this instruction has just been pushed into the task list and is waiting to be further processed. The request slot `cmdSlotTag` is used to identify the NVMe request to which this request belongs, and it is used to call back when the request is completed. Note that the insertion operation may fail because the task list may be full. You can set up a memory space to save these requests that cannot be sent in temporarily, or you can return the request directly like in the example. The `set_auto_nvme_cpl` function sets this NVMe request as completed, and when called, the host side will unblock, which has been in a blocked state until the call. 

![image-20230217211930669](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172119693.png)

4. After parsing the NVMe request, the firmware will enter the task list processing function `check_task_list`, which is on line 199 of `task_handler.c`. What a new application needs to do is to add a task processing program inside the for loop on line 205. For example, taking the `TEST` task as an example, we first check its status. If it is in the initialization state, we distribute the read requests for the block addresses it needs to read. This part can directly use the already written function `generate_child_task`. This function will directly distribute the block address request information written in the `task_entry` structure and directly set its status to `READING` after distribution. The `generate_child_task` function will automatically divide the request data length into multiple `FLASHREAD` instructions and increase the number of sub-tasks `child_task_count` of the parent task (which is the `TEST` task here). A `FLASHREAD` instruction is responsible for reading the data of one 16K block. Secondly, when the instruction becomes `READING`, we need to check whether its sub-tasks have been completed, and the number of sub-tasks controls our description in step 5. If the number of completed sub-tasks is greater than or equal to (under normal circumstances, it will not be greater, only equal, but we add greater than during the development phase to avoid some bugs), it means that the task has been completed, and we can set the NVMe instruction to complete and use `reset_task` to reset this task to accept the next task. The return of data is also completed using DMA, and this feature is to be supplemented.

![image-20230217213001529](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172130558.png)

5. The final step is to add the data processing function in `handle_task` (line 292 of `task_handler.c`). In the firmware of Cosmos, whenever a read flash memory request is completed, the content on line 694 of `low_level_scheduler.c` is called. We have inserted a `handle_task` function inside it. When a block of data is read up, we check whether this is a read request issued by NDP (judged by whether the `task_index` field of the FTL request has a value). If it is, we call the `handle_task` function and pass the cache address of the data in DRAM, task number, data offset, and other information to the data processing function. As shown in the figure, when the number of the completed read task is our `FLASHREAD` task, we further check whether the parent task of this read task is our `TEST` task. If it is, we print it out (we can also use a string search function to search for keywords, here we just simply print it out), and increase the number of completed sub-requests of the parent task, that is, `TEST`, by 1. Then, in the next task list traversal, it can be found that the sub-tasks have been completed and can be returned, as shown in step 4.

![image-20230217215051417](https://gitee.com/lijiali1101/picbed/raw/master/image/202302172150442.png)

