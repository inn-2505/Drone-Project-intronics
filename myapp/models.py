from django.db import models

class DroneStatus(models.Model):
    # status (ARM, DISARM, TAKEOFF, LAND)
    flight_mode = models.CharField(max_length=20, default="DISARM")
    
    # GPS coordinates and altitude
    latitude = models.DecimalField(max_digits=9, decimal_places=6, null=True, blank=True)
    longitude = models.DecimalField(max_digits=9, decimal_places=6, null=True, blank=True)
    altitude = models.DecimalField(max_digits=6, decimal_places=2, default=0.00) # เมตร
    
    # orientation and speed
    speed = models.DecimalField(max_digits=5, decimal_places=2, default=0.00) # m/s
    roll = models.DecimalField(max_digits=5, decimal_places=2, default=0.00)
    pitch = models.DecimalField(max_digits=5, decimal_places=2, default=0.00)
    yaw = models.DecimalField(max_digits=5, decimal_places=2, default=0.00)
    
    # battery status
    battery_voltage = models.DecimalField(max_digits=4, decimal_places=2, default=0.00) # โวลต์
    battery_percentage = models.IntegerField(default=100) # %
    
    # time when data was recorded in real-time
    timestamp = models.DateTimeField(auto_now_add=True)

    class Meta:
        db_table = 'drone_f722_status' 
        ordering = ['-timestamp'] 

    def __str__(self):
        return f"Drone_F722 - {self.flight_mode} at {self.timestamp}"

class DroneCommand(models.Model):
    # command (ARM, DISARM, TAKEOFF, LAND)
    command = models.CharField(max_length=50, default="NONE")
    # status of the command (PENDING, EXECUTED, FAILED)
    status = models.CharField(max_length=20, default="PENDING")
    timestamp = models.DateTimeField(auto_now=True)

    class Meta:
        db_table = 'drone_f722_command'    