from django.contrib import admin
from myapp.models import DroneStatus, DroneCommand

admin.site.register(DroneStatus)
admin.site.register(DroneCommand)