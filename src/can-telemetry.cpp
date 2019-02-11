#include "can-telemetry.h"

/** 
 * Constructor for the telematics object. 
 * @param &channel  CAN channel object
 * @param baud_rate desired baud_rate to listen at 
 * @param node_id   the ID of this telemetry node
 * @param timeout   timeout before returning sentinel values
 * @param debug     debug flag 
 */
CANTelemetry::CANTelemetry(CANChannel &channel, int baud_rate, 
                            int node_id, int timeout, bool debug) {
    _can = &channel;
    _baud_rate = baud_rate;
    _node_id = node_id;
    _timeout = (unsigned long)timeout;
    _debug = debug;
    if (_debug) {
        _can->begin(_baud_rate, CAN_TEST_MODE);
    } else {
        _can->begin(_baud_rate);
    }
    _set_mask();
}

/**
 * Poll the CAN bus, and returns the values as an unsigned 64-bit object.
 * So an unsigned long long, basically.
 * @param header    header to poll the bus with
 * @param filter    response pattern to listen from the bus
 * @param mode      mode to run at (CALL_AND_RESP or PASSIVE_POLL)
 * @param frame     frame mode (DATA_FRAME or REMOTE_FRAME)
 * @param payload   payload to send
 * @param len       length of the payload
 */
uint64_t CANTelemetry::poll(uint32_t header, uint32_t filter, int mode,
                            bool frame, uint8_t payload[], int len) {

    if (len > 8) return -1;

    // Set the mask and transmit our message
    _set_mask(filter);

    if (mode == CALL_AND_RESP) {
        // Create the packet and send it
        CANMessage message;
        message.id = header;
        message.len = len;
        message.rtr = frame;
        for (int i = 0; i < len; i++) {
            message.data[i] = payload[i];
        }
        _can->transmit(message);
    }

    // Wait for a response, or timeout
    if (_can->errorStatus() == CAN_NO_ERROR) {
        CANMessage response;
        unsigned long t_stamp = millis();
        while (true) {
            if (_can->receive(response)) {
                _set_mask();
                return _decode(response.data, response.len);
            }
            if (millis() - t_stamp >= _timeout) {
                _set_mask();
                return -1;
            }
        }
    } else {
        CANMessage clear;
        clear.id = _node_id;
        _can->transmit(clear); // Transmit to check error status
        _set_mask();
        return -1;
    }
}

/**
 * Overloaded method for same header and filter.
 * @param header    header and filter to poll the bus with
 * @param mode      mode to run at (CALL_AND_RESP or PASSIVE_POLL)
 * @param frame     frame mode (DATA_FRAME or REMOTE_FRAME)
 * @param payload   payload to send
 * @param len       length of the payload
 */
uint64_t CANTelemetry::poll(uint32_t header, int mode, bool frame,
                            uint8_t payload[], int len) {
    return poll(header, header, mode, frame, payload, len);
}

/**
 * Adjust the timeout for the poll before returning.
 */
void CANTelemetry::change_timeout(int timeout) {
    _timeout = (unsigned long)timeout;
}

/** 
 * Decode and return the bytes as an unsigned 64-bit value (unsigned long
 * long). This is a private helper method.
 */
uint64_t CANTelemetry::_decode(uint8_t * arr, int len) {
    uint8_t n[8] = {0};
    for (int i = 0; i < len; i++) {
        n[i] = arr[i];
    }
    return *(uint64_t *)n;
}

/**
 * Set the mask on the CAN stream. It defaults to 0. This is a private
 * helper method.
 */
void CANTelemetry::_set_mask(int mask) {
    _can->clearFilters();
    _can->addFilter(mask, 0x7FF);
}

/**
 * Set the mask on the CAN stream. It defaults to the node ID. Private.
 */
void CANTelemetry::_set_mask() {
    _set_mask(_node_id);
}