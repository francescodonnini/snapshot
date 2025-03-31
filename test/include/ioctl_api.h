#ifndef IOCTL_API_H
#define IOCTL_API_H

static const char *snapshot_strerror(int err) {
    switch (err)
    {
    case -5000:
        return "a device registered with this name already exists";
    case -5001:
        return "device name is too long";
    case -5002:
        return "device name or password are wrong";
    case -5003:
        return "device is already mounted";
    default:
        return "unknown error";
    }
}

int activate(const char *device, const char *password);

int deactivate(const char *device, const char *password);
#endif