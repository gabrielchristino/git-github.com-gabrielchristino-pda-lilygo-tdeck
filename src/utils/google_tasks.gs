/**
 * Google Apps Script to handle Google Tasks API integration.
 * Supports GET, POST, DELETE methods for listing, creating, updating, and deleting tasks.
 */

function doGet(e) {
  var tasks = Tasks.Tasks.list('@default').items;
  if (!tasks) {
    return ContentService.createTextOutput(JSON.stringify({items: []}))
      .setMimeType(ContentService.MimeType.JSON);
  }
  return ContentService.createTextOutput(JSON.stringify({items: tasks}))
    .setMimeType(ContentService.MimeType.JSON);
}

function doPost(e) {
  var data = JSON.parse(e.postData.contents);
  if (data.delete && data.id) {
    try {
      Tasks.Tasks.remove('@default', data.id);
      return ContentService.createTextOutput(JSON.stringify({success: true}))
        .setMimeType(ContentService.MimeType.JSON);
    } catch (err) {
      return ContentService.createTextOutput(JSON.stringify({error: err.message}))
        .setMimeType(ContentService.MimeType.JSON);
    }
  } else if (data.update && data.id && data.title) {
    var task = Tasks.Tasks.get('@default', data.id);
    if (!task) {
      return ContentService.createTextOutput(JSON.stringify({error: "Task not found"}))
        .setMimeType(ContentService.MimeType.JSON);
    }
    task.title = data.title;
    var updatedTask = Tasks.Tasks.update(task, '@default', data.id);
    return ContentService.createTextOutput(JSON.stringify(updatedTask))
      .setMimeType(ContentService.MimeType.JSON);
  } else if (data.title) {
    var task = Tasks.Tasks.insert({title: data.title}, '@default');
    return ContentService.createTextOutput(JSON.stringify(task))
      .setMimeType(ContentService.MimeType.JSON);
  } else {
    return ContentService.createTextOutput(JSON.stringify({error: "Missing title"}))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

function doDelete(e) {
  // Deprecated: deletion handled via POST with delete flag
  return ContentService.createTextOutput(JSON.stringify({error: "Use POST with delete flag"}))
    .setMimeType(ContentService.MimeType.JSON);
}
