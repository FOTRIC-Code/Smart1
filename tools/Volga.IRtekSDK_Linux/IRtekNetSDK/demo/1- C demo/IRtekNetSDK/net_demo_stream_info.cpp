#include "net_demo_stream_info.h"

#include <iostream>

void print_channels(NET_HANDLE handle, std::ostream& out)
{
    std::vector<net_channel_info> channels;
    USHORT count = 0;
    if (!get_channels(handle, channels, count, out))
    {
        return;
    }

    for (USHORT i = 0; i < count; ++i)
    {
        out << "channel[" << i << "]"
            << " id=" << channels[i].id
            << " name=" << as_cstr(channels[i].name)
            << "\n";
    }
}
