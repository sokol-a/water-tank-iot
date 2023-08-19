document.addEventListener("DOMContentLoaded", function() {
    function fetchTemperature() {
        fetch("/get-current-temp")
            .then(response => response.json())
            .then(data => {
                let temperature = data.temperature;
                let timestamp = new Date(data.timestamp * 1000); // Convert to milliseconds

                // Format the date-time string as per your preference
                let formattedDate = timestamp.toLocaleString();

                // Update the DOM elements
                document.getElementById("tempValue").innerText = temperature + "°C";
                document.getElementById("timestampValue").innerText = "Last updated: " + formattedDate;
            })
            .catch(error => {
                console.error("Error fetching current temperature:", error);
                document.getElementById("currentTemperature").innerText = "Failed to fetch temperature.";
            });

    }

    fetchTemperature();
    setInterval(fetchTemperature, 60000);  // Update every minute
});
