# KIOT Plugin: Notifications

Creates a Home Assistant notify entity per computer and shows what it receives as a KDE notification.

## Message

A plain text message is shown as it is. A JSON object can set more; all fields but `message` are optional:

| Field | Description |
|---|---|
| `message` | The text of the notification |
| `title` | Optional: the title (default: `kiot`) |
| `urgency` | Optional: `low`, `normal`, `high` or `critical` (default: `normal`). A critical notification is shown even in Do Not Disturb mode and stays until it is dismissed |
| `url` | Optional: opened when the notification is clicked, e.g. a Home Assistant dashboard |

Example, from a Home Assistant script or automation:

```yaml
action: notify.send_message
target:
  entity_id: notify.mycomputer_notifications
data:
  message: >-
    {{ {"title": "Washing machine", "message": "Done", "urgency": "high",
        "url": "https://homeassistant.local:8123/dashboard-home"} | to_json }}
```

## Notification settings

The notifications are kiot's own (`kiot.notifyrc`), so their sound, popup and history can be set in
System Settings → Notifications → Applications → Kiot.

In the Flatpak, notifications go through the notification portal: urgency and the click action work,
but the host's System Settings does not list kiot's notification events, and a clicked notification
may not stay in the history.
