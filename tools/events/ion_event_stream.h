/*
 * Copyright 2009-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at:
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#ifndef IONC_VALUE_STREAM_H
#define IONC_VALUE_STREAM_H

#include <vector>
#include <string>
#include "ion.h"

#define IONCLEANEXIT goto cleanup

/**
 * Conveys any errors that occurred down-stack.
 */
#define IONREPORT(x) \
    err = (x); \
    if (err) { \
        IONCLEANEXIT; \
    }

/**
 * Conveys an error upward at the point it occurs.
 */
#define IONERROR(type, code, msg, loc, idx, res) \
    _ion_event_set_error(res, type, code, msg, loc, idx, __FILE__, __LINE__); \
    IONREPORT(code)

/**
 * Conveys an illegal state error upward at the point it occurs.
 */
#define IONFAILSTATE(code, msg, res) IONERROR(ERROR_TYPE_STATE, code, msg, _error_location, _error_event_index, res)

#define IONCCALL(type, x, loc, idx, m) \
    err = (x); \
    if (err) { \
        IONERROR(type, err, m, loc, idx, result); \
    }

#define ION_SET_ERROR_CONTEXT(location, event_index) \
    std::string *_error_location = location; \
    size_t *_error_event_index = event_index;

/**
 * Conveys an error upward from a call to an ion-c read API.
 */
#define IONCREAD(x) IONCCALL(ERROR_TYPE_READ, x, _error_location, NULL, "")
#define IONCWRITE(x) IONCCALL(ERROR_TYPE_WRITE, x, _error_location, _error_event_index, "")
#define IONCSTATE(x, m) IONCCALL(ERROR_TYPE_STATE, x, _error_location, NULL, m)

#define ION_NON_FATAL(x, m) { \
    iERR err_backup = (x); \
    if (err == IERR_OK && err_backup != IERR_OK) { \
        err = err_backup; \
        _ion_event_set_error(result, ERROR_TYPE_STATE, err, m, _error_location, _error_event_index, __FILE__, __LINE__); \
    } \
}

#define cRETURN cleanup: RETURN(__location_name__, __line__, __count__++, err)

typedef ION_STRING ION_LOB;

typedef enum _ion_event_type {
    SCALAR = 0,
    CONTAINER_START,
    CONTAINER_END,
    SYMBOL_TABLE,
    STREAM_END,
    UNKNOWN
} ION_EVENT_TYPE;

class IonEvent {
public:
    ION_EVENT_TYPE event_type;
    ION_TYPE ion_type;
    ION_SYMBOL *field_name;
    ION_SYMBOL *annotations;
    SIZE num_annotations;
    int depth;
    void *value;

    IonEvent(ION_EVENT_TYPE event_type, ION_TYPE ion_type, ION_SYMBOL *field_name, ION_SYMBOL *annotations, SIZE num_annotations, int depth);
    ~IonEvent();
};

class IonEventStream {
    std::vector<IonEvent*> *event_stream;
public:
    std::string location;
    IonEventStream(std::string location="UNKNOWN");
    ~IonEventStream();

    /**
     * Creates a new IonEvent from the given parameters, appends it to the IonEventStream, and returns it.
     * It is up to the caller to set the returned IonEvent's value.
     */
    IonEvent *append_new(ION_EVENT_TYPE event_type, ION_TYPE ion_type, ION_SYMBOL *field_name,
                         ION_SYMBOL *annotations, SIZE num_annotations, int depth);

    size_t size() {
        return event_stream->size();
    }

    IonEvent *at(size_t index) {
        return event_stream->at(index);
    }

    void remove(size_t index) {
        event_stream->erase(event_stream->begin() + index);
    }
};

typedef enum _ion_event_output_type {
    OUTPUT_TYPE_TEXT_PRETTY = 0,
    OUTPUT_TYPE_TEXT_UGLY,
    OUTPUT_TYPE_BINARY,
    OUTPUT_TYPE_EVENTS,
    OUTPUT_TYPE_NONE,
} ION_EVENT_OUTPUT_TYPE;

typedef enum _ion_event_error_type {
    ERROR_TYPE_UNKNOWN = 0,
    ERROR_TYPE_READ,
    ERROR_TYPE_WRITE,
    ERROR_TYPE_STATE,
} ION_EVENT_ERROR_TYPE;

class IonEventErrorDescription {
public:
    ION_EVENT_ERROR_TYPE error_type;
    std::string message;
    std::string location;
    size_t event_index;
    bool has_location;
    bool has_event_index;

    IonEventErrorDescription() {
        memset(this, 0, sizeof(IonEventErrorDescription));
    }
};

typedef enum _ion_event_comparison_result_type {
    COMPARISON_RESULT_EQUAL = 0,
    COMPARISON_RESULT_NOT_EQUAL,
    COMPARISON_RESULT_ERROR
} ION_EVENT_COMPARISON_RESULT_TYPE;

class IonEventComparisonContext {
public:
    std::string location;
    IonEvent *event;
    size_t event_index;

    IonEventComparisonContext() {
        memset(this, 0, sizeof(IonEventComparisonContext));
    }
};

class IonEventComparisonResult {
public:
    ION_EVENT_COMPARISON_RESULT_TYPE result;
    IonEventComparisonContext lhs;
    IonEventComparisonContext rhs;
    std::string message;

    IonEventComparisonResult() {
        memset(this, 0, sizeof(IonEventComparisonResult));
    }
};

class IonEventResult {
public:
    IonEventErrorDescription error_description;
    IonEventComparisonResult comparison_result;
    bool has_error_description;
    bool has_comparison_result;

    IonEventResult() {
        memset(this, 0, sizeof(IonEventResult));
    }
};

class IonEventReport {
    std::vector<IonEventErrorDescription> error_report;
    std::vector<IonEventComparisonResult> comparison_report;
public:
    IonEventReport() {}
    ~IonEventReport() {
        for (size_t i = 0; i < comparison_report.size(); i++) {
            IonEventComparisonResult *comparison_result = &comparison_report.at(i);
            if (comparison_result->lhs.event) {
                delete comparison_result->lhs.event;
            }
            if (comparison_result->rhs.event) {
                delete comparison_result->rhs.event;
            }
        }
    }
    void addResult(IonEventResult *result);
    iERR writeErrorsTo(hWRITER writer);
    iERR writeComparisonResultsTo(hWRITER writer, std::string *location, ION_CATALOG *catalog, IonEventResult *result);
    bool hasErrors() {
        return !error_report.empty();
    }
    bool hasComparisonFailures() {
        return !comparison_report.empty();
    }
    std::vector<IonEventErrorDescription> *getErrors() { return &error_report; }
    std::vector<IonEventComparisonResult> *getComparisonResults() { return &comparison_report; }
};

iERR ion_event_stream_write_error_report(hWRITER writer, IonEventReport *report, std::string *location, ION_CATALOG *catalog, IonEventResult *result);
iERR ion_event_stream_write_comparison_report(hWRITER writer, IonEventReport *report, std::string *location, ION_CATALOG *catalog, IonEventResult *result);

/**
 * Configure the given reader options to add a SYMBOL_TABLE event to the given IonEventStream whenever the symbol
 * table context changes.
 */
void ion_event_register_symbol_table_callback(ION_READER_OPTIONS *options, IonEventStream *stream);

/**
 * Returns the length of the value starting at start_index, in number of events. Scalars will always return 1.
 */
size_t valueEventLength(IonEventStream *stream, size_t start_index);

/**
 * Returns the length of the stream starting at index and ending in a STREAM_END event, in number of events. Empty
 * streams will always return 1.
 */
size_t ion_event_stream_length(IonEventStream *stream, size_t index);

iERR ion_event_stream_read_imports(hREADER reader, ION_COLLECTION *imports, std::string *location, IonEventResult *result);

/**
 * Reads IonEvents from the given BYTE* of Ion data into the given IonEventStream.
 */
iERR read_value_stream_from_bytes(const BYTE *ion_string, SIZE len, IonEventStream *stream, ION_CATALOG *catalog, IonEventResult *result=NULL);

iERR ion_event_stream_read(hREADER hreader, IonEventStream *stream, ION_TYPE t, BOOL in_struct, int depth, BOOL is_embedded_stream_set, IonEventResult *result);

/**
 * Reads an IonEventStream from the given reader's data, which may contain a regular Ion stream or an event stream.
 */
iERR ion_event_stream_read_all(hREADER hreader, ION_CATALOG *catalog, IonEventStream *stream, IonEventResult *result);

iERR ion_event_stream_write_all_to_bytes(IonEventStream *stream, ION_EVENT_OUTPUT_TYPE output_type,
                                         ION_CATALOG *catalog, BYTE **out, SIZE *len, IonEventResult *result);

/**
 * Writes an IonEventStream as an Ion stream using the given writer.
 */
iERR ion_event_stream_write_all(hWRITER writer, IonEventStream *stream, IonEventResult *result);

/**
 * Writes an IonEventStream as a serialized event stream using the given writer.
 */
iERR ion_event_stream_write_all_events(hWRITER writer, IonEventStream *stream, ION_CATALOG *catalog, IonEventResult *result);

iERR ion_event_stream_write_error(hWRITER writer, IonEventErrorDescription *error_description);

iERR ion_event_stream_write_comparison_result(hWRITER writer, IonEventComparisonResult *comparison_result, std::string *location, ION_CATALOG *catalog, IonEventResult *result);

iERR ion_event_copy(IonEvent **dst, IonEvent *src, std::string location, IonEventResult *result);

#endif //IONC_VALUE_STREAM_H
