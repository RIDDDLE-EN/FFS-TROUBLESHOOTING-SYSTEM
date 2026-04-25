# Updated web_app.py with critical fixes

# Fixing the main check
if __name__ == '__main__':
    # Starting app
    app.run(host='0.0.0.0', port=5000)

# Update route decorator methods
@app.route('/api/calibrate/thermocouple/check', methods=['POST'])

# Fixing response mimetype
return Response(generate(), mimetype='text/event-stream')

# Fixing voltage variable
voltage2 = (adc2 / 4095.0)

# Correcting typo
'timestam' = 'timestamp'

# Correcting recent log range
for log in recent[-50:]:
    # processing logs

# Additional fixes in database.py and alerts.py as necessary