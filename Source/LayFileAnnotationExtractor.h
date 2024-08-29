#pragma once

#include <JuceHeader.h>
#include <memory>

struct Annotation
{
    double timestamp;
    double duration; 
    int durationInt;
    int eventType;
    String text;

    Annotation(double t, double d, int di, int et, const String& txt) : 
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
    bool operator==(const Annotation& other) const
    {
        return (timestamp == other.timestamp) &&
            (duration == other.duration) &&
            (durationInt == other.durationInt) &&
            (eventType == other.eventType) &&
            (text == other.text);
    }
};

class LayFileAnnotationExtractor
{
public:
    void OpenFile(const String& filePath);

    Array<Annotation> GetNewAnnotations(const Array<Annotation>& existingAnnotations) const;

    void SetPosition(int position) { mPosition = position; }

private:
    void ReadCommentsSection();
    Annotation ParseAnnotation(const String& line) const;

    String Trim(const String& str);

private:
    Array<Annotation> mAnnotations;
    std::unique_ptr<FileInputStream> mInputStream;
    int mPosition{ 0 };
};