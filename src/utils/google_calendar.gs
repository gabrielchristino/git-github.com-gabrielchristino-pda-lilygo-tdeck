/**
 * Google Apps Script to handle Google Calendar API integration.
 * Supports GET, POST methods for listing, creating, updating, and deleting events.
 */

function doGet(e) {
  var calendar = CalendarApp.getDefaultCalendar();
  var now = new Date();
  var oneMonthLater = new Date();
  oneMonthLater.setMonth(now.getMonth() + 1);

  var events = calendar.getEvents(now, oneMonthLater);
  var eventList = [];

  for (var i = 0; i < events.length; i++) {
    var event = events[i];
    eventList.push({
      id: event.getId(),
      title: event.getTitle(),
      startTime: event.getStartTime(),
      endTime: event.getEndTime(),
      description: event.getDescription(),
      location: event.getLocation()
    });
  }

  return ContentService.createTextOutput(JSON.stringify({items: eventList}))
    .setMimeType(ContentService.MimeType.JSON);
}

function doPost(e) {
  var data = JSON.parse(e.postData.contents);
  var calendar = CalendarApp.getDefaultCalendar();

  if (data.delete && data.id) {
    try {
      var event = calendar.getEventById(data.id);
      if (event) {
        event.deleteEvent();
        return ContentService.createTextOutput(JSON.stringify({success: true}))
          .setMimeType(ContentService.MimeType.JSON);
      } else {
        return ContentService.createTextOutput(JSON.stringify({error: "Event not found"}))
          .setMimeType(ContentService.MimeType.JSON);
      }
    } catch (err) {
      return ContentService.createTextOutput(JSON.stringify({error: err.message}))
        .setMimeType(ContentService.MimeType.JSON);
    }
  } else if (data.update && data.id) {
    try {
      var event = calendar.getEventById(data.id);
      if (!event) {
        return ContentService.createTextOutput(JSON.stringify({error: "Event not found"}))
          .setMimeType(ContentService.MimeType.JSON);
      }
      if (data.title) event.setTitle(data.title);
      if (data.startTime) event.setTime(new Date(data.startTime), new Date(data.endTime));
      if (data.description) event.setDescription(data.description);
      if (data.location) event.setLocation(data.location);
      return ContentService.createTextOutput(JSON.stringify({success: true}))
        .setMimeType(ContentService.MimeType.JSON);
    } catch (err) {
      return ContentService.createTextOutput(JSON.stringify({error: err.message}))
        .setMimeType(ContentService.MimeType.JSON);
    }
  } else if (data.title && data.startTime && data.endTime) {
    try {
      var newEvent = calendar.createEvent(
        data.title,
        new Date(data.startTime),
        new Date(data.endTime),
        {
          description: data.description || '',
          location: data.location || ''
        }
      );
      return ContentService.createTextOutput(JSON.stringify({
        id: newEvent.getId(),
        title: newEvent.getTitle(),
        startTime: newEvent.getStartTime(),
        endTime: newEvent.getEndTime(),
        description: newEvent.getDescription(),
        location: newEvent.getLocation()
      })).setMimeType(ContentService.MimeType.JSON);
    } catch (err) {
      return ContentService.createTextOutput(JSON.stringify({error: err.message}))
        .setMimeType(ContentService.MimeType.JSON);
    }
  } else {
    return ContentService.createTextOutput(JSON.stringify({error: "Missing required fields"}))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
