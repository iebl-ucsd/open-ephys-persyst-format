#pragma once

#include <JuceHeader.h>
#include <memory>

struct Comment
{
    double timestamp;
    double duration; 
    int durationInt;
    int eventType;
    String text;

    Comment(double t, double d, int di, int et, const String& txt) : 
        timestamp(t), 
        duration(d),
        durationInt(di), 
        eventType(et), 
        text(txt)
    {}

    // Convert to string representation (for comparison or display)
    String ToString() const
    {
        return String(timestamp) + "," + String(duration) + "," +
            String(durationInt) + "," + String(eventType) + "," + text;
    }

    // Equality comparison based on the entire content of the comment
    bool operator==(const Comment& other) const
    {
        return (timestamp == other.timestamp) &&
            (duration == other.duration) &&
            (durationInt == other.durationInt) &&
            (eventType == other.eventType) &&
            (text == other.text);
    }
};