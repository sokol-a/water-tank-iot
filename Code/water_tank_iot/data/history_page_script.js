document.addEventListener("DOMContentLoaded", function() {
    // Helper function to parse CSV data
    function parseCSV(data) {
        let rows = data.trim().split("\n");
        let result = rows.map(row => {
            let [timestamp, temperature] = row.split(",");
            return {
                timestamp: parseInt(timestamp, 10),
                temperature: parseFloat(temperature)
            };
        });
        return result;
    }

    // Fetch the temperature history data from the server
    fetch("/get-temperature-history")
        .then(response => response.text())
        .then(data => {
            // Parse the CSV data
            let parsedData = parseCSV(data);
            
            // Extract timestamps and temperatures from the parsed data
            let timestamps = parsedData.map(entry => new Date(entry.timestamp * 1000));
            let temperatures = parsedData.map(entry => entry.temperature);

            // Define the plot data and layout
            let plotData = [{
                x: timestamps,
                y: temperatures,
                type: 'scatter'
            }];
            let layout = {
                title: 'Temperature History',
                xaxis: {
                    title: {text:'Timestamp', standoff: 25},
                    tickangle: 45,  // Rotate x-axis labels by 45 degrees
                    tickformat: '%d/%m %H:%M'// Display as "Day-Month Hour:Minute"
                },
                yaxis: {
                    title: 'Temperature (°C)'
                }
            };

            // Render the plot using Plotly
            Plotly.newPlot('plotlyDiv', plotData, layout);
        })
        .catch(error => {
            console.error("Error fetching temperature history:", error);
            document.getElementById("plotlyDiv").innerText = "Failed to fetch temperature history.";
        });
});
