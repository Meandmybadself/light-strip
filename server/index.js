const express = require('express');
const parser = require('cron-parser');

const app = express();
const port = process.env.PORT || 4001;

// Array of cron expressions
const cronSchedules = [
  '30 6 * * 1-5',  // Every weekday at 6:30AM
  '0 8 * * 1-5' // Every weekday at 8:00AM
];

const calculateNextTimeInSeconds = () => {
  const now = new Date();
  let nextTime = null;

  for (const schedule of cronSchedules) {
    try {
      const interval = parser.parseExpression(schedule);
      const next = interval.next().toDate();
      
      if (!nextTime || next < nextTime) {
        nextTime = next;
      }
    } catch (err) {
      console.error(`Invalid cron expression: ${schedule}`);
    }
  }

  if (!nextTime) {
    throw new Error('No valid next time found');
  }

  return Math.floor((nextTime.getTime() - now.getTime()) / 1000);
};

app.get('/', (_, res) => {
  try {
    const secondsUntilNext = calculateNextTimeInSeconds();
    res.send(`${secondsUntilNext}`);
  } catch (error) {
    res.status(500).json({ error: 'Failed to calculate next time' });
  }
});

// Testing endpoint
app.get('/test/:seconds', (req, res) => {
  res.send(`${req.params.seconds}`);
});

app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
}); 