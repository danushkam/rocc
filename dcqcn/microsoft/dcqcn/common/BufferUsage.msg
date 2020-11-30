namespace dcqcn;

packet BufferUsage
{
    int portNum; // ingress port number
    int usage;   // usage in bytes; +ve if allocated and -ve if released
}