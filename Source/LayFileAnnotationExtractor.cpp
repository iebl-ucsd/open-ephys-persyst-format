#include "LayFileAnnotationExtractor.h"

#include <RecordingLib.h>

Array<Annotation> LayFileAnnotationExtractor::GetNewAnnotations(const Array<Annotation>& existingAnnotations) const
{
    Array<Annotation> newAnnotations;

    for (const auto& annotation : mAnnotations)
    {
        if (!existingAnnotations.contains(annotation))
            newAnnotations.add(annotation);
    }

    return newAnnotations;
}

void LayFileAnnotationExtractor::OpenFile(const String& filePath)
{
    mInputStream = std::make_unique<FileInputStream>(filePath);

    if (!mInputStream->openedOk())
    {
        LOGC("Failed to open file: ", filePath);
        return;
    }
}

void LayFileAnnotationExtractor::ReadCommentsSection()
{
    mAnnotations.clear();
    mInputStream->setPosition(mPosition);

    bool inAnnotations = false;

    while (!mInputStream->isExhausted())
    {
        String line = mInputStream->readNextLine();
        String trimmedLine = Trim(line);

        if (trimmedLine == "[Comments]")
        {
            inAnnotations = true;
            continue;
        }

        if (trimmedLine.isEmpty() || trimmedLine.startsWithChar('['))
        {
            // Exit if a new section begins or there's an empty line
            if (inAnnotations) 
                break;
        }

        if (inAnnotations && !trimmedLine.isEmpty())
            mAnnotations.add(ParseAnnotation(trimmedLine));
    }
}

Annotation LayFileAnnotationExtractor::ParseAnnotation(const String& line) const
{
    StringArray tokens;
    tokens.addTokens(line, ",", "");

    double timestamp = tokens[0].getDoubleValue();
    double duration = tokens[1].getDoubleValue();  // Parsing duration as double
    int durationInt = tokens[2].getIntValue();
    int eventType = tokens[3].getIntValue();
    String text = tokens[4];

    return Annotation(timestamp, duration, durationInt, eventType, text);
}

String LayFileAnnotationExtractor::Trim(const String& str)
{
    return str.trim();
}
