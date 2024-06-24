#include "ReceiverThread.h"
#include "SDLWindow.h"

ReceiverThread::ReceiverThread(ThreadSafeQueue<FrameData>& queue) : frameQueue(queue)
{
    this->running = false;
}

ReceiverThread::~ReceiverThread()
{
    if(this->running)
    {
        this->stop();
    }
}

void ReceiverThread::start()
{
    if (this->running) 
    {
        std::cerr << "Receiver thread is already running" << std::endl;
        return;
    }

    std::cout << "Starting Receiver Thread!" << std::endl;
    this->running = true;
    receiverThread = std::thread(&ReceiverThread::worker, this);
}

void ReceiverThread::stop()
{
    if (!this->running) 
    {
        std::cerr << "Receiver thread is already stopped" << std::endl;
        return;
    }

    std::cout << "Stopping Receiver Thread!" << std::endl;
    this->running = false;

    //std::cout << "Thread Joinable: " << this->receiverThread.joinable() << std::endl;
    if (this->receiverThread.joinable())
    {
        //std::cout << "Joining..." << std::endl;
        this->receiverThread.join();
        //std::cout << "Joined Successfully!" << std::endl;
    }
}

void ReceiverThread::worker() 
{
    auto startTime = std::chrono::steady_clock::now();
    auto lastReportTime = startTime;
    
    UDPReceive receiver;
    receiver.init(2000);

    UDPSend sender;
    char ip[] = "127.0.0.1";
	sender.init(ip, 10000);
    
    const int bufferSize = 3000000;
    char buffer[bufferSize];
    
    double packetTime;
    int receivedBytes;

    int totalReceivedBytes = 0;
    int receivedPackets = 0;
    int lostPackets = 0;

    Decoder decoder = Decoder(AV_CODEC_ID_H264);
    decoder.addFrameDecodedListener(std::bind(&ReceiverThread::onFrameDecodedCallback, this, std::placeholders::_1));

    while(running) 
    {
        receivedBytes = receiver.receive(buffer, bufferSize, &packetTime);

        if (receivedBytes > 0) 
        {
            //std::cout << "Received payload size: " << receivedBytes << " bytes" << std::endl;
            totalReceivedBytes += receivedBytes;
            receivedPackets++;

            if (running) 
            {
                decoder.decode(buffer, receivedBytes);
            }
        } 
        else 
        {
            std::cerr << "Failed to receive packet." << std::endl;
            lostPackets++;
        }

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedSeconds = currentTime - startTime;

        std::chrono::duration<double> timeSinceLastReport = currentTime - lastReportTime;
        if(timeSinceLastReport.count() > 5.0)
        {
            //calculate metrics
            double receivedByteRate = totalReceivedBytes / elapsedSeconds.count();
            double packetLossRate = static_cast<double>(lostPackets) / receivedPackets;
            double frameRate = receivedPackets / elapsedSeconds.count();

            //serialize and send the report
            std::cout << std::endl << "Sending Report!" << std::endl;
            std::cout << "ReceivedByteRate: " << receivedByteRate << std::endl;
            std::cout << "PacketLossRate: " << packetLossRate << std::endl;
            std::cout << "frameRate: " << frameRate << std::endl << std::endl;

            this->currentFramerate = frameRate;
            ReportData report = {receivedByteRate, packetLossRate, frameRate};
            char reportBuffer[3*sizeof(double)];
            report.serialize(reportBuffer);
            int bytesSent = sender.send(reportBuffer, sizeof(reportBuffer));
            std::cout << "Sent Bytes: " << bytesSent << std::endl;

            lastReportTime = currentTime;
        }
    }
}

void ReceiverThread::onFrameDecodedCallback(const AVFrame* frame)
{
    if(this->running)
    {
        //std::cout << "Pushing Frame into Queue!" << std::endl;

        IMGUtility utility;
        int width = 0;
        int height = 0;
        uint32_t* image_data = nullptr;
        image_data = utility.extractImageData(frame, &width, &height);

        FrameData frameData = {image_data, width, height, this->currentFramerate};
        this->frameQueue.push(frameData);
    }
}